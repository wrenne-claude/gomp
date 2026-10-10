# gomp_core: adapter, model and platform layers. A static QML module (Gomp.Core) so model types
# are registered declaratively, and linkable by the tests without the UI.
qt_add_library(gomp_core STATIC)
qt_add_qml_module(gomp_core
    URI Gomp.Core
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    OUTPUT_DIRECTORY ${PROJECT_BINARY_DIR}/qml/Gomp/Core
    SOURCES
        adapter/AgentEvents.h
        adapter/AgentBackend.h
        adapter/JsonLineProcess.h adapter/JsonLineProcess.cpp
        adapter/RpcParser.h adapter/RpcParser.cpp
        adapter/RpcBackend.h adapter/RpcBackend.cpp
        adapter/AcpParser.h adapter/AcpParser.cpp
        adapter/AcpBackend.h adapter/AcpBackend.cpp
        adapter/OmpBackend.h adapter/OmpBackend.cpp
        adapter/MockBackend.h adapter/MockBackend.cpp
        adapter/OmpLocator.h adapter/OmpLocator.cpp
        adapter/OmpCli.h adapter/OmpCli.cpp
        util/Markdown.h util/Markdown.cpp
        util/SyntaxHighlighter.h util/SyntaxHighlighter.cpp
        util/LineDiff.h util/LineDiff.cpp
        util/FuzzyMatch.h util/FuzzyMatch.cpp
        util/YamlLite.h util/YamlLite.cpp
        util/Keyring.h util/Keyring.cpp
        util/Notifier.h util/Notifier.cpp
        util/GitInfo.h util/GitInfo.cpp
        util/Format.h util/Format.cpp
        util/IconProvider.h util/IconProvider.cpp
        config/SettingsSchema.h config/SettingsSchema.cpp
        config/ConfigStore.h config/ConfigStore.cpp
        config/McpStore.h config/McpStore.cpp
        config/ProviderStore.h config/ProviderStore.cpp
        model/ConversationModel.h model/ConversationModel.cpp
        model/TodoModel.h model/TodoModel.cpp
        model/SubagentModel.h model/SubagentModel.cpp
        model/CommandModel.h model/CommandModel.cpp
        model/Workspace.h model/Workspace.cpp
        model/WorkspaceManager.h model/WorkspaceManager.cpp
        model/SessionIndex.h model/SessionIndex.cpp
        model/SessionTreeModel.h model/SessionTreeModel.cpp
        model/ModelCatalog.h model/ModelCatalog.cpp
        model/SettingsModel.h model/SettingsModel.cpp
        model/FileIndex.h model/FileIndex.cpp
        model/PaletteModel.h model/PaletteModel.cpp
        model/ShortcutModel.h model/ShortcutModel.cpp
        app/Preferences.h app/Preferences.cpp
        app/AppController.h app/AppController.cpp
        app/EffectsProbe.h app/EffectsProbe.cpp
)
qt_add_resources(gomp_core "gomp_data"
    PREFIX /gomp
    BASE ${PROJECT_SOURCE_DIR}/resources
    FILES
        ${PROJECT_SOURCE_DIR}/resources/schema/omp-settings.json
        ${PROJECT_SOURCE_DIR}/resources/mock/explain.json
        ${PROJECT_SOURCE_DIR}/resources/mock/edit.json
        ${PROJECT_SOURCE_DIR}/resources/mock/plan.json
        ${PROJECT_SOURCE_DIR}/resources/mock/subagents.json
        ${PROJECT_SOURCE_DIR}/resources/mock/error.json
        ${PROJECT_SOURCE_DIR}/resources/mock/sessions/api-audit.jsonl
        ${PROJECT_SOURCE_DIR}/resources/mock/sessions/api-ratelimit.jsonl
        ${PROJECT_SOURCE_DIR}/resources/mock/sessions/api-redis.jsonl
        ${PROJECT_SOURCE_DIR}/resources/mock/sessions/cli-flags.jsonl
        ${PROJECT_SOURCE_DIR}/resources/mock/sessions/cli-release.jsonl
        ${PROJECT_SOURCE_DIR}/resources/mock/sessions/site-tokens.jsonl
)
target_include_directories(gomp_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
# qmltyperegistrar includes registered headers by file name.
target_include_directories(gomp_core PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/adapter ${CMAKE_CURRENT_SOURCE_DIR}/util ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/model ${CMAKE_CURRENT_SOURCE_DIR}/app)
target_link_libraries(gomp_core
    PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::DBus Qt6::Concurrent Qt6::Svg
    PRIVATE gomp_warnings)
