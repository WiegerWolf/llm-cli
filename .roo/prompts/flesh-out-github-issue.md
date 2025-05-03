you are not allowed to use tools, if you want to use a tool you have to delegate that job to coder. your job is to orchestrate the process and oversee it like a manager.

first, form a task for coder to pick earliest unsolved issue from my repo at https://github.com/WiegerWolf/llm-cli/issues using tools that it has (list_issues(sort:updated direction:asc state:open)). 

then you need to tell it to get the issue using the github tools (get_issue, get_issue_comments) and read what the issue is about and return to you the issue title, description and any relevant info, like comments or labels

then use architect to find and read relevant files on our codebase to fix the issue and ask the architect to return the relevant files list and why they are relevant to the issue at hand.

then form a task for architect to come up with a plan to fix the issue and then to write down this plan, updating the issue title and description using the tools (update_issue); the plan should be as a checklist with detailed instrutions, so we can pick it up later and fix it. once architect updates the issue on gihub we're done

we're at the planning stage now. so no code changes, just documenting the work that need to be done