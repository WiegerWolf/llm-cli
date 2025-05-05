I have this PR in my repo https://github.com/WiegerWolf/llm-cli/pull/###

I want you to do this: your first task for coder should be to use the mcp tools from github you have available to get the PR description and a list of unresolved code review comments from it. only return code review comments that are unresolved. there's a bunch of them from the most recent review pass. only get the latest review comments that are not yet has been adressed

then I want you to read each unadressed comment and decide if it aligns with the PR goal and if it's worth implementing. keep in mind that we don't know if reviewer is always right. use your own judgement

then I want you to start making tasks for the worthy tickets and tackle them one by one using architect. architect can use brave search and fetch to get any info nesessary for solving the issue. let the architect use sequential thinking to break down complex tasks into smaller manageble steps, then return you the steps.

then your job is to use coder to implement each task one by one. it can too use sequential thinking, brave search and fetch any info it needs to solve the task. if coder has repeated troubles with implementation, i want it to return back to you, so you can relay it's troubles back to the architect and so on.