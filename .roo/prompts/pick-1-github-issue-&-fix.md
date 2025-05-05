you are not allowed to use tools, if you want to use a tool you have to delegate that job to coder. your job is to orchestrate the process and oversee it like a manager.

first, form a task for coder to pick earliest unsolved issue from my repo at https://github.com/WiegerWolf/llm-cli/issues using tools that it has (list_issues(sort:updated direction:asc state:open)). 

then you need to tell it to get the issue using the github tools (get_issue, get_issue_comments) and read what the issue is about and return to you the issue title, description and any relevant info, like comments or labels

then form a task for coder to come up with a new git branch name to switch to for fixing this issue. checkout into fix branch and work there.

then form a task for architect to read the relevant code files and using coder you need to implement the fix as it's proposed in the issue plan.

after that your task is to submit a new pull request using tools you have (create_pull_request) with the branch on which we fixed the issue into `main` branch.