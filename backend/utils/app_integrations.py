import json
import threading
from typing import Optional, List
import uuid
import base64
import requests
import time
from datetime import datetime

import database.notifications as notification_db
from database import mem_db
from database import redis_db
from database.apps import record_app_usage, get_private_apps_db, get_public_apps_db
from database.chat import add_app_message, get_app_messages
from database.redis_db import get_generic_cache, set_generic_cache, get_enabled_apps
from models.app import App, UsageHistoryType
from models.chat import Message
from models.conversation import Conversation, ConversationSource
from models.notification_message import NotificationMessage
from utils.apps import get_available_apps
from utils.notifications import send_notification
from utils.llm.clients import generate_embedding, llm_mini
from utils.llm.proactive_notification import get_proactive_message
from database.vector_db import query_vectors_by_metadata
import database.conversations as conversations_db
from utils.llm.openglass import process_openglass_image, process_openglass_image_with_prompt

PROACTIVE_NOTI_LIMIT_SECONDS = 30  # 1 noti / 30s


def get_github_docs_content(repo="BasedHardware/omi", path="docs/docs"):
    """
    Recursively retrieves content from GitHub docs folder and subfolders using GitHub API.
    Returns a dict mapping file paths to their raw content.

    If cached, returns cached content. (24 hours)
    So any changes to the docs will take 24 hours to be reflected.
    """
    if cached := get_generic_cache(f'get_github_docs_content_{repo}_{path}'):
        return cached
    docs_content = {}
    headers = {"Authorization": f"token {os.getenv('GITHUB_TOKEN')}"}

    def get_contents(path):
        url = f"https://api.github.com/repos/{repo}/contents/{path}"
        response = requests.get(url, headers=headers)

        if response.status_code != 200:
            print(f"Failed to fetch contents for {path}: {response.status_code}")
            return

        contents = response.json()

        if not isinstance(contents, list):
            return

        for item in contents:
            if item["type"] == "file" and (item["name"].endswith(".md") or item["name"].endswith(".mdx")):
                # Get raw content for documentation files
                raw_response = requests.get(item["download_url"], headers=headers)
                if raw_response.status_code == 200:
                    docs_content[item["path"]] = raw_response.text

            elif item["type"] == "dir":
                # Recursively process subfolders
                get_contents(item["path"])

    get_contents(path)
    set_generic_cache(f'get_github_docs_content_{repo}_{path}', docs_content, 60 * 24 * 7)
    return docs_content


# **************************************************
# ************* EXTERNAL INTEGRATIONS **************
# **************************************************

def trigger_external_integrations(uid: str, conversation: Conversation) -> list:
    """ON CONVERSATION CREATED"""
    if not conversation or conversation.discarded:
        return []

    apps: List[App] = get_available_apps(uid)
    filtered_apps = [app for app in apps if
                     app.triggers_on_conversation_creation() and app.enabled]
    if not filtered_apps:
        return []

    threads = []
    results = {}

    def _single(app: App):
        if not app.external_integration.webhook_url:
            return

        conversation_dict = conversation.as_dict_cleaned_dates()

        # Ignore external data on workflow
        if conversation.source == ConversationSource.workflow and 'external_data' in conversation_dict:
            conversation_dict['external_data'] = None

        url = app.external_integration.webhook_url
        if '?' in url:
            url += '&uid=' + uid
        else:
            url += '?uid=' + uid

        try:
            response = requests.post(url, json=conversation_dict, timeout=30, )  # TODO: failing?
            if response.status_code != 200:
                print('App integration failed', app.id, 'status:', response.status_code, 'result:', response.text[:100])
                return

            if app.uid is not None:
                if app.uid != uid:
                    record_app_usage(uid, app.id, UsageHistoryType.memory_created_external_integration,
                                     conversation_id=conversation.id)
            else:
                record_app_usage(uid, app.id, UsageHistoryType.memory_created_external_integration,
                                 conversation_id=conversation.id)

            # print('response', response.json())
            if message := response.json().get('message', ''):
                results[app.id] = message
        except Exception as e:
            print(f"Plugin integration error: {e}")
            return

    for app in filtered_apps:
        threads.append(threading.Thread(target=_single, args=(app,)))

    [t.start() for t in threads]
    [t.join() for t in threads]

    messages = []
    for key, message in results.items():
        if not message:
            continue
        messages.append(add_app_message(message, key, uid, conversation.id))
    return messages


async def trigger_realtime_integrations(uid: str, segments: list[dict], conversation_id: str | None):
    print("trigger_realtime_integrations", uid)
    """REALTIME STREAMING"""
    # TODO: don't retrieve token before knowing if to notify
    token = notification_db.get_token_only(uid)
    _trigger_realtime_integrations(uid, token, segments, conversation_id)


async def trigger_realtime_audio_bytes(uid: str, sample_rate: int, data: bytearray):
    print("trigger_realtime_audio_bytes", uid)
    """REALTIME AUDIO STREAMING"""
    _trigger_realtime_audio_bytes(uid, sample_rate, data)


# proactive notification
def _retrieve_contextual_memories(uid: str, user_context):
    vector = (
        generate_embedding(user_context.get('question', ''))
        if user_context.get('question')
        else [0] * 3072
    )
    print("query_vectors vector:", vector[:5])

    date_filters = {}  # not support yet
    filters = user_context.get('filters', {})
    memories_id = query_vectors_by_metadata(
        uid,
        vector,
        dates_filter=[date_filters.get("start"), date_filters.get("end")],
        people=filters.get("people", []),
        topics=filters.get("topics", []),
        entities=filters.get("entities", []),
        dates=filters.get("dates", []),
    )
    return conversations_db.get_conversations_by_id(uid, memories_id)


def _hit_proactive_notification_rate_limits(uid: str, app: App):
    sent_at = mem_db.get_proactive_noti_sent_at(uid, app.id)
    if sent_at and time.time() - sent_at < PROACTIVE_NOTI_LIMIT_SECONDS:
        return True

    # remote
    sent_at = redis_db.get_proactive_noti_sent_at(uid, app.id)
    if not sent_at:
        return False
    ttl = redis_db.get_proactive_noti_sent_at_ttl(uid, app.id)
    if ttl > 0:
        mem_db.set_proactive_noti_sent_at(uid, app.id, int(time.time() + ttl), ttl=ttl)

    return time.time() - sent_at < PROACTIVE_NOTI_LIMIT_SECONDS


def _set_proactive_noti_sent_at(uid: str, app: App):
    ts = time.time()
    mem_db.set_proactive_noti_sent_at(uid, app, int(ts), ttl=PROACTIVE_NOTI_LIMIT_SECONDS)
    redis_db.set_proactive_noti_sent_at(uid, app.id, int(ts), ttl=PROACTIVE_NOTI_LIMIT_SECONDS)


def _process_proactive_notification(uid: str, token: str, app: App, data):
    if not app.has_capability("proactive_notification") or not data:
        print(f"App {app.id} is not proactive_notification or data invalid", uid)
        return None

    # rate limits
    if _hit_proactive_notification_rate_limits(uid, app):
        print(f"App {app.id} is reach rate limits 1 noti per user per {PROACTIVE_NOTI_LIMIT_SECONDS}s", uid)
        return None

    max_prompt_char_limit = 128000
    min_message_char_limit = 5

    prompt = data.get('prompt', '')
    if len(prompt) > max_prompt_char_limit:
        send_app_notification(token, app.name, app.id,
                                 f"Prompt too long: {len(prompt)}/{max_prompt_char_limit} characters. Please shorten.")
        print(f"App {app.id}, prompt too long, length: {len(prompt)}/{max_prompt_char_limit}", uid)
        return None

    filter_scopes = app.filter_proactive_notification_scopes(data.get('params', []))

    # context
    context = None
    if 'user_context' in filter_scopes:
        memories = _retrieve_contextual_memories(uid, data.get('context', {}))
        if len(memories) > 0:
            context = Conversation.conversations_to_string(memories)

    # messages
    messages = []
    if 'user_chat' in filter_scopes:
        messages = list(reversed([Message(**msg) for msg in get_app_messages(uid, app.id, limit=10)]))

    # print(f'_process_proactive_notification context {context[:100] if context else "empty"}')

    # retrive message
    message = get_proactive_message(uid, prompt, filter_scopes, context, messages)
    if not message or len(message) < min_message_char_limit:
        print(f"Plugins {app.id}, message too short", uid)
        return None

    # send notification
    send_app_notification(token, app.name, app.id, message)

    # set rate
    _set_proactive_noti_sent_at(uid, app)
    return message


def _trigger_realtime_audio_bytes(uid: str, sample_rate: int, data: bytearray):
    apps: List[App] = get_available_apps(uid)
    filtered_apps = [
        app for app in apps if
        app.triggers_realtime_audio_bytes() and app.enabled
    ]
    if not filtered_apps:
        return {}

    threads = []
    results = {}

    def _single(app: App):
        if not app.external_integration.webhook_url:
            return

        url = app.external_integration.webhook_url
        url += f'?sample_rate={sample_rate}&uid={uid}'
        try:
            response = requests.post(url, data=data, headers={'Content-Type': 'application/octet-stream'}, timeout=15)
            print('trigger_realtime_audio_bytes', app.id, 'status:', response.status_code)
        except Exception as e:
            print(f"Plugin integration error: {e}")
            return

    for app in filtered_apps:
        threads.append(threading.Thread(target=_single, args=(app,)))

    [t.start() for t in threads]
    [t.join() for t in threads]

    return results


def _trigger_realtime_integrations(uid: str, token: str, segments: List[dict], conversation_id: str | None) -> dict:
    apps: List[App] = get_available_apps(uid)
    filtered_apps = [
        app for app in apps if
        app.triggers_realtime() and app.enabled
    ]
    if not filtered_apps:
        return {}

    threads = []
    results = {}

    def _single(app: App):
        if not app.external_integration.webhook_url:
            return

        url = app.external_integration.webhook_url
        if '?' in url:
            url += '&uid=' + uid
        else:
            url += '?uid=' + uid

        try:
            response = requests.post(url, json={"session_id": uid, "segments": segments}, timeout=30)
            if response.status_code != 200:
                print('trigger_realtime_integrations', app.id, 'status: ', response.status_code, 'results:',
                      response.text[:100])
                return

            if (app.uid is None or app.uid != uid) and conversation_id is not None:
                record_app_usage(uid, app.id, UsageHistoryType.transcript_processed_external_integration, conversation_id=conversation_id)

            response_data = response.json()
            if not response_data:
                return

            # message
            message = response_data.get('message', '')
            # print('Plugin', plugin.id, 'response message:', message)
            if message and len(message) > 5:
                send_app_notification(token, app.name, app.id, message)
                results[app.id] = message

            # proactive_notification
            noti = response_data.get('notification', None)
            # print('Plugin', plugin.id, 'response notification:', noti)
            if app.has_capability("proactive_notification"):
                message = _process_proactive_notification(uid, token, app, noti)
                if message:
                    results[app.id] = message

        except Exception as e:
            print(f"App integration error: {e}")
            return

    for app in filtered_apps:
        threads.append(threading.Thread(target=_single, args=(app,)))

    [t.start() for t in threads]
    [t.join() for t in threads]
    messages = []
    for key, message in results.items():
        if not message:
            continue
        messages.append(add_app_message(message, key, uid))

    return messages


def send_app_notification(token: str, app_name: str, app_id: str, message: str):
    ai_message = NotificationMessage(
        text=message,
        app_id=app_id,
        from_integration='true',
        type='text',
        notification_type='plugin',
        navigate_to=f'/chat/{app_id}',
    )

    send_notification(token, app_name + ' says', message, NotificationMessage.get_message_as_dict(ai_message))


def trigger_openglass_apps(uid: str, image_id: str, image_description: str, image_url: str) -> List[dict]:
    """
    Trigger enabled openGlass apps for a user when an image is captured.
    Clean and simple approach using the existing app infrastructure.
    """
    # Get all enabled apps for this user
    user_enabled_apps = get_enabled_apps(uid)
    private_apps = get_private_apps_db(uid) 
    public_apps = get_public_apps_db(uid)
    all_apps = private_apps + [app for app in public_apps if app.get('enabled', False)]
    
    # Filter for openglass apps that are enabled
    openglass_apps = [
        app for app in all_apps 
        if 'openglass' in app.get('capabilities', []) and app.get('id') in user_enabled_apps and app.get('openglass_prompt')
    ]
    
    if not openglass_apps:
        return []
    
    results = []
    for app in openglass_apps:
        try:
            app_name = app.get('name', 'Unknown App')
            prompt = app.get('openglass_prompt', '')
            confidence_threshold = app.get('openglass_confidence_threshold', 0.7)
            
            # Enhanced prompt with confidence scoring
            full_prompt = f"""{prompt}

Image description: {image_description}

INSTRUCTIONS:
- Analyze the image description for content relevant to your purpose
- Rate your confidence in the analysis on a scale of 0.0 to 1.0
- If confidence is below {confidence_threshold}, respond with: "LOW_CONFIDENCE"
- Otherwise, provide your analysis followed by "CONFIDENCE: X.X" on the last line
- Be specific about what you can identify before providing analysis
- Only analyze content that is clearly visible and identifiable

Example response format:
[Your detailed analysis here...]
CONFIDENCE: 0.85"""
             
            # Call LLM
            response = process_openglass_image_with_prompt(full_prompt, image_url)
            
            # Confidence-based filtering
            response_lower = response.lower()
            should_display = True
            
            # Check for explicit low confidence response
            if 'low_confidence' in response_lower:
                should_display = False
            else:
                # Try to extract confidence score
                confidence_score = None
                if 'confidence:' in response_lower:
                    try:
                        confidence_line = [line for line in response.split('\n') if 'confidence:' in line.lower()][-1]
                        confidence_score = float(confidence_line.split(':')[-1].strip())
                    except (ValueError, IndexError):
                        pass
                
                # Apply confidence threshold
                if confidence_score is not None:
                    if confidence_score < confidence_threshold:
                        should_display = False
                else:
                    # Fallback filtering for non-scored responses
                    generic_filters = [
                        'not relevant', 'no relevant', 'nothing relevant', 'cannot identify',
                        'unable to identify', 'not present', 'not visible', 'nothing visible',
                        'not applicable', 'n/a', 'no content', 'nothing to analyze'
                    ]
                    if any(filter_word in response_lower for filter_word in generic_filters) or len(response.strip()) < 15:
                        should_display = False
            
            # Determine if notification is needed
            should_notify = any(keyword in response_lower for keyword in ['alert:', 'warning:', 'important:', 'notification:'])
            
            result = {
                'app_name': app_name,
                'message': response,
                'should_display': should_display,
                'should_notify': should_notify,
                'timestamp': datetime.now().isoformat(),
                'data': {}
            }
            
            results.append(result)
            
        except Exception as e:
            print(f"Error processing openGlass app {app.get('name', 'Unknown')}: {e}")
            continue
    
    return results


def process_openglass_image_with_prompt(prompt: str, image_url: str) -> str:
    """
    Process an openGlass image with a custom app prompt.
    Returns the LLM response for analysis.
    """
    try:
        response = llm_mini.invoke(prompt)
        return response.content.strip()
    except Exception as e:
        print(f"Error processing openGlass image with prompt: {e}")
        return f"Error processing image: {str(e)}"
