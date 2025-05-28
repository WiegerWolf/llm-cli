# Chronological Layout User Guide

## Introduction

The chronological layout feature provides an intelligent way to visualize conversation history by combining the natural clustering of force-directed algorithms with time-aware positioning. This guide will help you understand how to use and configure the chronological layout for optimal message history visualization.

## Getting Started

### Enabling Chronological Layout

The chronological layout is enabled by default in the graph view. When you switch to graph mode in the GUI, messages are automatically arranged using the hybrid chronological-force algorithm.

### Basic Operation

1. **Switch to Graph View**: In the GUI, select the graph view mode
2. **Automatic Layout**: Messages are automatically positioned based on their timestamps and relationships
3. **Interactive Navigation**: Use mouse controls to pan and zoom through the conversation graph

## Understanding the Layout

### Visual Principles

#### Chronological Ordering
- **Vertical Arrangement**: Older messages appear higher, newer messages lower
- **Time Flow**: Natural top-to-bottom reading follows conversation chronology
- **Temporal Spacing**: Time gaps between messages are reflected in vertical spacing

#### Relationship Clustering
- **Connected Messages**: Related messages (replies, follow-ups) cluster together
- **Conversation Threads**: Branched discussions spread horizontally while maintaining chronological order
- **Visual Connections**: Lines connect related messages showing conversation flow

#### Spatial Organization
- **Depth Positioning**: Message depth in conversation threads affects horizontal positioning
- **Balanced Layout**: Algorithm balances chronological order with readable spacing
- **Overlap Prevention**: Messages maintain minimum distances for readability

### Layout Behavior

#### Linear Conversations
```
[Message 1] ← Oldest (top)
    ↓
[Message 2]
    ↓
[Message 3]
    ↓
[Message 4] ← Newest (bottom)
```

#### Branched Conversations
```
[Root Message]
    ↓
[Reply 1] ← [Reply 2] ← [Reply 3]
    ↓
[Follow-up]
```

## Configuration Options

### Basic Parameters

#### Chronological Strength
- **Purpose**: Controls how strictly chronological order is maintained
- **Range**: 0.0 (disabled) to 1.0 (strict)
- **Default**: 0.1 (moderate)
- **Effect**: Higher values create more rigid chronological ordering

#### Vertical Bias
- **Purpose**: Influences the vertical arrangement preference
- **Range**: 0.0 (no bias) to 1.0 (strong vertical bias)
- **Default**: 0.3 (moderate vertical preference)
- **Effect**: Higher values create more vertical, timeline-like layouts

#### Chronological Spacing
- **Purpose**: Sets the base vertical distance between chronologically adjacent messages
- **Range**: 50-500 pixels
- **Default**: 150 pixels
- **Effect**: Larger values create more spread-out timelines

### Advanced Parameters

#### Force Balance
- **Spring Strength**: Controls attraction between related messages
- **Repulsion Strength**: Controls spacing between all messages
- **Damping Factor**: Controls animation smoothness and stability

#### Animation Settings
- **Time Step**: Controls animation speed and smoothness
- **Max Iterations**: Limits computation time for large graphs
- **Convergence Threshold**: Determines when layout is considered stable

## Usage Scenarios

### Scenario 1: Linear Chat History

**Best Settings:**
- Chronological Strength: 0.2-0.3
- Vertical Bias: 0.4-0.6
- Chronological Spacing: 120-180 pixels

**Expected Result:**
- Messages arranged in clear vertical timeline
- Consistent spacing between messages
- Easy to follow conversation flow

### Scenario 2: Branched Discussions

**Best Settings:**
- Chronological Strength: 0.1-0.2
- Vertical Bias: 0.2-0.4
- Chronological Spacing: 150-200 pixels

**Expected Result:**
- Main thread maintains chronological order
- Branches spread horizontally
- Related discussions cluster together

### Scenario 3: Complex Multi-threaded Conversations

**Best Settings:**
- Chronological Strength: 0.05-0.15
- Vertical Bias: 0.2-0.3
- Chronological Spacing: 100-150 pixels

**Expected Result:**
- Multiple conversation threads visible
- Chronological order preserved within threads
- Clear visual separation between topics

### Scenario 4: Long-term Conversation History

**Best Settings:**
- Chronological Strength: 0.15-0.25
- Vertical Bias: 0.3-0.5
- Chronological Spacing: 80-120 pixels

**Expected Result:**
- Compressed timeline for long histories
- Major time gaps clearly visible
- Conversation evolution apparent

## Parameter Tuning Guide

### Step-by-Step Tuning

#### Step 1: Assess Your Data
1. **Conversation Type**: Linear vs. branched vs. complex
2. **Time Span**: Minutes, hours, days, or longer
3. **Message Density**: Frequent exchanges vs. sparse communication
4. **Thread Complexity**: Simple replies vs. multi-topic discussions

#### Step 2: Start with Presets
Choose the appropriate scenario settings from above as your starting point.

#### Step 3: Fine-tune Chronological Parameters

**If chronological order is too loose:**
- Increase `temporal_strength` by 0.05-0.1
- Increase `vertical_bias` by 0.1-0.2

**If layout is too rigid:**
- Decrease `temporal_strength` by 0.05
- Decrease `vertical_bias` by 0.1

**If messages are too close together:**
- Increase `chronological_spacing` by 20-50 pixels

**If messages are too spread out:**
- Decrease `chronological_spacing` by 20-50 pixels

#### Step 4: Adjust Force Parameters

**If related messages are too far apart:**
- Increase `spring_strength` by 0.01-0.02

**If messages overlap or cluster too tightly:**
- Increase `repulsion_strength` by 5000-10000
- Increase `min_distance` by 20-50 pixels

**If layout is unstable or oscillating:**
- Increase `damping_factor` by 0.05-0.1
- Decrease `time_step` by 0.002-0.005

### Common Adjustments

| Issue | Parameter | Adjustment |
|-------|-----------|------------|
| Messages out of chronological order | `temporal_strength` | Increase by 0.05-0.1 |
| Layout too vertical/rigid | `vertical_bias` | Decrease by 0.1-0.2 |
| Messages too close together | `chronological_spacing` | Increase by 20-50px |
| Related messages too far apart | `spring_strength` | Increase by 0.01-0.02 |
| Messages overlapping | `repulsion_strength` | Increase by 5000-10000 |
| Animation too fast/jerky | `time_step` | Decrease by 0.002-0.005 |
| Layout takes too long | `max_iterations` | Decrease by 100-200 |

## Visual Results and Interpretation

### Successful Layout Indicators

#### Good Chronological Order
- Older messages consistently appear above newer ones
- Time gaps are visually apparent in spacing
- Conversation flow is easy to follow

#### Proper Clustering
- Related messages are grouped together
- Conversation threads are visually distinct
- Branching points are clear

#### Readable Spacing
- No overlapping messages
- Consistent minimum distances
- Comfortable reading experience

### Troubleshooting Visual Issues

#### Poor Chronological Order
**Symptoms:**
- Messages appear in wrong temporal sequence
- Time flow is confusing or reversed
- Recent messages appear above older ones

**Solutions:**
1. Increase `temporal_strength` to 0.2-0.3
2. Increase `vertical_bias` to 0.4-0.6
3. Verify message timestamps are correct

#### Excessive Clustering
**Symptoms:**
- Messages overlap or are too close
- Text is difficult to read
- Layout appears cramped

**Solutions:**
1. Increase `repulsion_strength` to 60000-80000
2. Increase `min_distance` to 250-300 pixels
3. Increase `chronological_spacing` to 200-250 pixels

#### Poor Thread Separation
**Symptoms:**
- Different conversation topics mix together
- Branching is not visually clear
- Related messages are scattered

**Solutions:**
1. Increase `spring_strength` to 0.08-0.12
2. Adjust `ideal_edge_length` to 350-450 pixels
3. Decrease `temporal_strength` slightly to allow more clustering

## Performance Considerations

### Optimization for Large Conversations

#### Parameter Adjustments for Performance
- Reduce `max_iterations` to 200-300 for faster computation
- Increase `convergence_threshold` to 0.2-0.5 for earlier stopping
- Use larger `time_step` (0.01-0.015) for faster animation

#### Memory and CPU Usage
- **Small conversations** (5-20 messages): Minimal impact
- **Medium conversations** (20-50 messages): Moderate CPU usage during layout
- **Large conversations** (50+ messages): Consider batch processing or simplified parameters

### Real-time vs. Batch Processing

#### Real-time Layout (Interactive)
- Use moderate iteration limits (300-500)
- Enable animation for user feedback
- Allow user interruption of long computations

#### Batch Layout (Background)
- Use higher iteration limits (500-1000)
- Optimize for quality over speed
- Process during idle time or startup

## Integration with Other Features

### Search and Filtering
- Layout adapts when messages are filtered
- Search results maintain chronological context
- Hidden messages don't affect layout computation

### Message Threading
- Parent-child relationships influence clustering
- Thread depth affects horizontal positioning
- Reply chains maintain chronological flow

### Export and Sharing
- Layout positions can be saved and restored
- Screenshots preserve spatial relationships
- Layout parameters can be shared between users

## Best Practices

### For Optimal User Experience

1. **Start Simple**: Begin with default parameters and adjust gradually
2. **Consider Context**: Match parameters to conversation type and user needs
3. **Test Iteratively**: Make small adjustments and observe results
4. **Document Settings**: Save successful parameter combinations for reuse
5. **User Feedback**: Allow users to adjust parameters based on their preferences

### For Development Integration

1. **Parameter Validation**: Ensure parameters are within reasonable ranges
2. **Performance Monitoring**: Track layout computation time and optimize as needed
3. **User Interface**: Provide intuitive controls for common adjustments
4. **Presets**: Offer predefined parameter sets for common use cases
5. **Help Integration**: Link parameter controls to relevant documentation sections

## Conclusion

The chronological layout algorithm provides a powerful tool for visualizing conversation history with both temporal awareness and natural clustering. By understanding the parameters and following the tuning guidelines in this guide, you can create layouts that are both visually appealing and functionally effective for your specific use cases.

Remember that the best layout parameters depend on your specific data characteristics and user needs. Experiment with different settings and use the troubleshooting guide to achieve optimal results for your conversation visualization requirements.