import 'package:flutter/material.dart';
import 'package:omi/providers/capture_provider.dart';
import 'package:provider/provider.dart';

class MarketplaceResultsWidget extends StatelessWidget {
  final String imageId;
  final bool isCompact;

  const MarketplaceResultsWidget({
    Key? key,
    required this.imageId,
    this.isCompact = false,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Consumer<CaptureProvider>(
      builder: (context, captureProvider, child) {
        final marketplaceResults = captureProvider.getMarketplaceResults(imageId);
        
        if (marketplaceResults == null || marketplaceResults.isEmpty) {
          return const SizedBox.shrink();
        }

        return Container(
          margin: const EdgeInsets.only(top: 8.0),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              // Elegant header
              Row(
                children: [
                  Icon(
                    Icons.auto_awesome,
                    size: 16,
                    color: Colors.indigo.shade600,
                  ),
                  const SizedBox(width: 8),
                  Text(
                    'AI Insights',
                    style: TextStyle(
                      fontSize: 15,
                      fontWeight: FontWeight.w600,
                      color: Colors.grey.shade800,
                      letterSpacing: -0.2,
                    ),
                  ),
                ],
              ),
              const SizedBox(height: 12),
              // Refined results
              ...marketplaceResults.map((result) => _buildGenericResultItem(result)).toList(),
            ],
          ),
        );
      },
    );
  }

  Widget _buildGenericResultItem(Map<String, dynamic> result) {
    final appName = result['app'] ?? 'Smart Assistant';
    final message = result['message'] ?? '';
    
    return Container(
      margin: const EdgeInsets.only(bottom: 10.0),
      padding: const EdgeInsets.all(16.0),
      decoration: BoxDecoration(
        color: Colors.grey.shade50,
        borderRadius: BorderRadius.circular(12.0),
        border: Border.all(color: Colors.grey.shade200),
        boxShadow: [
          BoxShadow(
            color: Colors.black.withOpacity(0.03),
            blurRadius: 4,
            offset: const Offset(0, 1),
          ),
        ],
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Text(
            appName,
            style: TextStyle(
              fontSize: 14,
              fontWeight: FontWeight.w600,
              color: Colors.grey.shade800,
              letterSpacing: -0.1,
            ),
          ),
          const SizedBox(height: 6),
          Text(
            message,
            style: TextStyle(
              fontSize: 13,
              height: 1.4,
              color: Colors.grey.shade700,
              fontWeight: FontWeight.w400,
            ),
          ),
        ],
      ),
    );
  }
}

class MarketplaceResultsExpanded extends StatelessWidget {
  final String imageId;

  const MarketplaceResultsExpanded({
    Key? key,
    required this.imageId,
  }) : super(key: key);

  @override
  Widget build(BuildContext context) {
    return Consumer<CaptureProvider>(
      builder: (context, captureProvider, child) {
        final marketplaceResults = captureProvider.getMarketplaceResults(imageId);
        
        if (marketplaceResults == null || marketplaceResults.isEmpty) {
          return const SizedBox.shrink();
        }

        return Container(
          padding: const EdgeInsets.all(16.0),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                'AI Insights',
                style: TextStyle(
                  fontSize: 18,
                  fontWeight: FontWeight.bold,
                  color: Colors.blue.shade700,
                ),
              ),
              const SizedBox(height: 16),
              ...marketplaceResults.map((result) => _buildExpandedResultItem(result)).toList(),
            ],
          ),
        );
      },
    );
  }

  Widget _buildExpandedResultItem(Map<String, dynamic> result) {
    final appName = result['app'] ?? 'Smart Assistant';
    final message = result['message'] ?? '';
    
    return Container(
      margin: const EdgeInsets.only(bottom: 12.0),
      padding: const EdgeInsets.all(16.0),
      decoration: BoxDecoration(
        color: Colors.blue.shade50,
        borderRadius: BorderRadius.circular(12.0),
        border: Border.all(color: Colors.blue.shade200),
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              Icon(
                Icons.smart_toy,
                size: 20,
                color: Colors.blue.shade600,
              ),
              const SizedBox(width: 8),
              Text(
                appName,
                style: TextStyle(
                  fontSize: 16,
                  fontWeight: FontWeight.w600,
                  color: Colors.blue.shade700,
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          Text(
            message,
            style: const TextStyle(
              fontSize: 15,
              height: 1.4,
            ),
          ),
        ],
      ),
    );
  }
} 