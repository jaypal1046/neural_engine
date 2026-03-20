@echo off
echo ============================================================
echo  Building neural_engine.exe - Self-Independent AI Edition
echo  Includes: Internet Learning + Self-Learning + All Features
echo ============================================================
cd src
g++ -O3 -std=c++17 -Wall -march=native -msse2 -mavx2 -mf16c -DUNIFIED_BUILD -DINCLUDE_SMART_BRAIN -I../include -o ../bin/neural_engine.exe unified_main.cpp agent_brain.cpp main.cpp test_block_access.cpp neural_engine.cpp compressor.cpp lz77.cpp huffman.cpp ans.cpp bwt.cpp bwt_simd.cpp ppm.cpp cmix.cpp knowledge_manager.cpp web_fetcher.cpp html_parser.cpp vector_index.cpp persistent_mixer.cpp compressed_knowledge.cpp block_access.cpp word_tokenizer.cpp word_ppm.cpp embedding_trainer.cpp rag_engine.cpp conversation_memory.cpp reasoning_engine.cpp bpe_tokenizer.cpp real_embeddings.cpp mini_transformer.cpp optimizer.cpp loss.cpp transformer_gradients.cpp precision_utils.cpp kv_cache.cpp mixed_precision.cpp mixed_precision_commands.cpp data_loader.cpp tensor_ops.cpp flash_attention.cpp self_learning.cpp self_learning_bridge.cpp internet_learning.cpp teacher_brain.cpp cloud_teacher.cpp quantize_commands.cpp quantization.cpp rlhf.cpp advanced_reasoning.cpp -lwinhttp -lws2_32 -pthread
cd ..
echo ============================================================
if exist bin\neural_engine.exe (
    echo BUILD SUCCESS! Neural Engine ^& Agentic AI ready.
    echo.
    echo Primary Local Brain Command - No API Key needed:
    echo   bin\neural_engine.exe agent_task "Analyze the project"
    echo   neural_engine.exe agent_task "Research quantum computing" --model ollama
    echo.
    echo Self-Independent AI Commands:
    echo   neural_engine.exe internet_learn           - Auto-learn from Wikipedia
    echo   neural_engine.exe internet_learn --daemon   - Continuous learning mode
    echo   neural_engine.exe learn_topic "AI"          - Learn specific topic
    echo   neural_engine.exe learn_url "https://..."   - Learn from any URL
    echo   neural_engine.exe auto_learn --daemon       - Self-improvement daemon
    echo   neural_engine.exe offline_bootstrap         - Train from 15M offline baseline ^(no internet^)
    echo   neural_engine.exe cloud_bootstrap           - Learn structurally perfect concepts from Claude 3.5
    echo   neural_engine.exe cloud_ask "prompt..."     - Interactively answer via Claude
    echo   neural_engine.exe self_learn_stats          - Show learning stats
) else (
    echo BUILD FAILED! Check errors above.
)
echo ============================================================
