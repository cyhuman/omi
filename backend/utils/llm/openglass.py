from typing import List

from models.conversation import ConversationPhoto, Structured
from utils.llm.clients import llm_mini


def summarize_open_glass(photos: List[ConversationPhoto]) -> Structured:
    photos_str = ''
    for i, photo in enumerate(photos):
        photos_str += f'{i + 1}. "{photo.description}"\n'
    prompt = f'''The user took a series of pictures from his POV, generated a description for each photo, and wants to create a memory from them.

      For the title, use the main topic/activity of the scenes (keep it concise, 3-8 words).
      For the overview, create a detailed summary that captures the essence of what the user was doing, where they were, and what was happening. This will serve as the main summary/description that the user sees. Make it engaging and descriptive, highlighting key activities, environment, context, and any interesting details from the visual scenes.
      For the category, classify the scenes into one of the available categories based on the main activity or context.

      Photos Descriptions: ```{photos_str}```
      '''.replace('    ', '').strip()
    return llm_mini.with_structured_output(Structured).invoke(prompt)


def process_openglass_image(prompt: str, image_description: str, app_name: str, app_id: str) -> dict:
    """
    Process an openGlass image using a custom app prompt.
    Simple and elegant approach that combines the app prompt with the image description.
    """
    try:
        # Combine the app prompt with the image context
        full_prompt = f"""
{prompt}

You are processing an image from openGlass smart glasses. Here's what the user is seeing:

Image Description: {image_description}

Provide your analysis based on your prompt above. Be concise but helpful.
Format your response as a brief message that will be shown to the user.
""".strip()

        # Use the mini LLM for fast processing
        response = llm_mini.invoke(full_prompt)
        
        if response and response.content:
            message = response.content.strip()
            
            # Determine if this should trigger a notification (simple heuristic)
            should_notify = any(keyword in message.lower() for keyword in [
                'alert', 'warning', 'important', 'urgent', 'detected', 'found'
            ])
            
            return {
                'message': message,
                'should_notify': should_notify,
                'data': {
                    'app_id': app_id,
                    'app_name': app_name,
                    'confidence': 0.85,  # Static confidence for now
                    'processing_time': 'fast'
                }
            }
        else:
            return {
                'message': f'{app_name} processed the image successfully',
                'should_notify': False,
                'data': {'app_id': app_id, 'app_name': app_name}
            }
            
    except Exception as e:
        print(f"Error in process_openglass_image for app {app_name}: {e}")
        return {
            'message': f'Error processing image: {str(e)}',
            'should_notify': False,
            'data': {'error': True, 'app_id': app_id, 'app_name': app_name}
        }


def process_openglass_image_with_prompt(prompt: str, image_url: str) -> str:
    """
    Process an openGlass image with a custom app prompt.
    Returns the LLM response which can be analyzed for conditional display.
    """
    try:
        response = llm_mini(
            messages=[
                {
                    'role': 'user', 
                    'content': prompt
                }
            ]
        )
        return response.strip()
    except Exception as e:
        print(f"Error processing openGlass image with prompt: {e}")
        return f"Error processing image: {str(e)}"