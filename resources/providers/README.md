# Provider logos

Drop one logo per provider into this folder. The Settings > Providers sign-in grid picks them up on
the next build (re-run `cmake --build build`; no code changes).

## Naming

`<provider-id>.svg`, or `<provider-id>.png` if you have no SVG. The id is omp's provider id,
lower case, exactly as in the table below: `anthropic.svg`, `github-copilot.svg`, `llama.cpp.svg`.
If both formats exist, the SVG is used. A provider without a file shows its initials instead.

## Format

- Square artwork, transparent background. It is drawn at 30x30 px on a dark or light tile, so it
  should read on both; a full-colour mark works, a pure black or pure white one does not.
- SVG: a `viewBox`, no embedded raster images, no external fonts.
- PNG: at least 120x120 px (it is shown at up to 2x pixel density).
- Colours are kept as supplied; gomp does not recolour provider logos.
- Check each vendor's brand guidelines before redistributing a logo in a public repository.

## Provider ids

Taken from `omp`'s login provider list (`get_login_providers`). Several tiles can share one brand
(the `xiaomi-*` plans, `siliconflow-cn`); copy the file under each id you want covered.

| File | Provider |
|---|---|
| `openai-codex.svg` | ChatGPT Plus/Pro (Codex Subscription) |
| `anthropic.svg` | Anthropic (Claude Pro/Max) |
| `zai.svg` | Z.AI (GLM Coding Plan) |
| `zai-coding-plan.svg` | Z.AI (GLM Coding Plan · Sign in) |
| `kimi-code.svg` | Kimi Code |
| `openrouter.svg` | OpenRouter |
| `github-copilot.svg` | GitHub Copilot |
| `cursor.svg` | Cursor (Claude, GPT, etc.) |
| `devin.svg` | Devin |
| `google-antigravity.svg` | Antigravity (Gemini 3, Claude, GPT-OSS) |
| `google-gemini-cli.svg` | Google Cloud Code Assist (Gemini CLI) |
| `openai-codex-device.svg` | ChatGPT Plus/Pro (Codex, headless/device) |
| `xai.svg` | xAI API |
| `xai-oauth.svg` | xAI Grok OAuth (SuperGrok or X Premium+) |
| `gitlab-duo.svg` | GitLab Duo Non-Agentic |
| `gitlab-duo-agent.svg` | GitLab Duo Agent |
| `alibaba-coding-plan.svg` | Alibaba Coding Plan |
| `alibaba-token-plan.svg` | QwenCloud Token Plan |
| `aiand.svg` | ai& |
| `abliteration.svg` | Abliteration |
| `zhipu-coding-plan.svg` | Zhipu Coding Plan |
| `umans.svg` | Umans AI Coding Plan |
| `qwen-portal.svg` | Qwen Portal |
| `sakana.svg` | Sakana AI |
| `minimax-code.svg` | MiniMax Token Plan (International) |
| `minimax-code-cn.svg` | MiniMax Token Plan (China) |
| `xiaomi.svg` | Xiaomi MiMo |
| `xiaomi-token-plan-sgp.svg` | Xiaomi Token Plan (Singapore) |
| `xiaomi-token-plan-ams.svg` | Xiaomi Token Plan (Europe) |
| `xiaomi-token-plan-cn.svg` | Xiaomi Token Plan (China) |
| `firepass.svg` | Fire Pass (Fireworks subscription) |
| `cline-pass.svg` | ClinePass |
| `factory-droid.svg` | Factory Droid |
| `commandcode.svg` | Command Code |
| `charm-hyper.svg` | Charm Hyper |
| `deepseek.svg` | DeepSeek |
| `stepfun.svg` | StepFun |
| `muse-code.svg` | Muse Code (Subscription) |
| `meta.svg` | Meta Model API |
| `moonshot.svg` | Moonshot (Kimi API) |
| `cerebras.svg` | Cerebras |
| `baseten.svg` | Baseten |
| `helmcode.svg` | Helmcode |
| `fireworks.svg` | Fireworks |
| `together.svg` | Together |
| `nvidia.svg` | NVIDIA |
| `novita.svg` | Novita |
| `deepinfra.svg` | DeepInfra |
| `huggingface.svg` | Hugging Face Inference |
| `perplexity.svg` | Perplexity (Pro/Max) |
| `qianfan.svg` | Qianfan |
| `venice.svg` | Venice |
| `siliconflow.svg` | SiliconFlow |
| `siliconflow-cn.svg` | SiliconFlow (China) |
| `synthetic.svg` | Synthetic |
| `nanogpt.svg` | NanoGPT |
| `wafer-serverless.svg` | Wafer Serverless |
| `coreweave.svg` | CoreWeave Serverless Inference |
| `vercel-ai-gateway.svg` | Vercel AI Gateway |
| `cloudflare-ai-gateway.svg` | Cloudflare AI Gateway |
| `snowflake.svg` | Snowflake Cortex |
| `litellm.svg` | LiteLLM |
| `kilo.svg` | Kilo Gateway |
| `zenmux.svg` | ZenMux |
| `opencode-zen.svg` | OpenCode Zen |
| `opencode-go.svg` | OpenCode Go |
| `yolo-auto.svg` | Yolo-Auto |
| `tavily.svg` | Tavily |
| `kagi.svg` | Kagi |
| `exa.svg` | Exa |
| `parallel.svg` | Parallel |
| `typesafe.svg` | TypeSafe |
| `ollama.svg` | Ollama (local) |
| `ollama-cloud.svg` | Ollama Cloud |
| `lm-studio.svg` | LM Studio (local) |
| `llama.cpp.svg` | llama.cpp (local) |
| `vllm.svg` | vLLM (local) |
| `gmi-cloud.svg` | GMI Cloud |
| `stencil.svg` | Stencil |
| `singularityapi-dev.svg` | SingularityAPI |
| `singularityapi-tech.svg` | SingularityAPI Reserved Lanes |
| `coralbricks.svg` | CoralBricks |

New ids appear when omp adds providers; the grid shows them with initials until a file is added.
