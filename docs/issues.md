# GUI Issues

*   User messages are not displayed in the log window; only response messages appear.
*   The "Clear History" button is unnecessary and should be removed.
*   The log window content does not wrap dynamically when the window is resized, causing a horizontal scrollbar to appear. Text should wrap to fit the window width.
*   The input field loses focus after sending a message. Focus should remain in the input field.
*   The input field does not have focus when the application starts. It should have initial focus.
*   The GUI struggles with character encoding, displaying '?' for characters like apostrophes. It needs proper UTF-8/Unicode support to handle various languages and symbols.
*   Status bar messages (like "Initializing", "Ready.") should be moved into the main history log as they occur, instead of only showing the last status in the dedicated status bar. The status bar itself can potentially be removed or repurposed.
*   Different message types (user input, model response, tool usage, status updates) need distinct visual styling in the history log for better readability.