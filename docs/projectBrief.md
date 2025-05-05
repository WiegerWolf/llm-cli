# projectBrief.md  
2025-05-04 01:46:00 – initial creation

## One-Sentence Summary  
A local C++ chat assistant that offers both a command-line (`llm-cli`) and graphical (`llm-gui`) interface to OpenRouter-based LLMs (e.g., GPT-4.1) and ships with built-in multi-step web-research tools.

---

## Problem Statement  
Most LLM front-ends are cloud-hosted, closed-source, or browser(electron)-bound.  
Developers and power users often need:  

2. A lightweight native GUI for everyday use.  
3. Reliable web-research capabilities that go beyond a single “search” call.  
4. Full control over API keys and conversation history on their own machine.

---

## Project Goals & Core Requirements  

| ID | Goal / Requirement | Priority |
|----|--------------------|----------|
| G1 | Provide a CLI binary (`llm-cli`) with fast, readline-driven chat. | Must |
| G2 | Provide a GUI binary (`llm-gui`) using Dear ImGui + GLFW. | Must |
| G3 | Connect to OpenRouter (default GPT-4.1-nano) via HTTPS (`libcurl`). | Must |
| G4 | Support tool calls (OpenAI function-calling API). | Must |
| G5 | Offer web-research tools: `search_web`, `visit_url`, `web_research`, `deep_research`. | Must |
| G6 | Persist conversation history in a local SQLite DB. | Should |
| G7 | Load API keys from a `.env` file; allow compile-time embedding. | Should |
| G8 | Keep UI responsive through multi-threading (GUI main loop + worker). | Should |
| G10| Scriptable install & build (`build.sh`, `install.sh`). | Should |
| G11| Make the code base extendable: easy addition of new tools. | Should |

---

## Non-Functional Requirements  

1. Modern C++17 minimum; C++20 features permitted when portable.  
2. Low on system resources (CPU, memory, disk).
4. GUI must remain usable on modest hardware (integrated GPUs).  
5. Build time under 2 minutes on a 4-core laptop (release mode).  
6. Memory footprint: ≤ 100 MB at idle for `llm-gui`.  

---

## Out-of-Scope (v1)  

- In-browser/WebAssembly build.  
- Mobile apps.  
- Fine-tuning / on-device model hosting.  
- Proprietary search engines or pay-walled data sources.  

---

## Primary Users / Stakeholders  

| Role               | Interest / Benefit                          |
|--------------------|---------------------------------------------|
| General desktop users | Simple GUI, privacy, rich web tools.     |
| OSS contributors   | Modular C++ code base, clear architecture.  |

---

## Success Criteria / KPIs  

1. `llm-cli --version` and `llm-gui --version` run on Linux/macOS/Windows without manual tweaks.  
2. End-to-end conversation with GPT-4 via OpenRouter completes in < 3 s (network latency permitting).  
3. Web-research query returns synthesized answer with ≥ 3 distinct sources cited.  
---

## Risks & Mitigations  

| Risk                               | Mitigation                                     |
|------------------------------------|------------------------------------------------|
| API-key leakage in logs / binaries | `.env` loading by default; compile-time keys optional but flagged “insecure”. |
| HTML parsing breaks due to site changes | Use multiple engines (Brave, DuckDuckGo) and keep Gumbo parsing generic. |
| GUI thread deadlocks               | Adopt well-tested producer/consumer queue with timeouts; add unit tests. |

---
