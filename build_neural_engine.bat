@echo off
cd src
g++ -O3 -std=c++17 -Wall -march=native -msse2 -mavx2 -DINCLUDE_SMART_BRAIN -I../include -o ../bin/neural_engine.exe unified_main.cpp main.cpp compressor.cpp lz77.cpp huffman.cpp ans.cpp bwt.cpp bwt_simd.cpp ppm.cpp cmix.cpp neural_engine.cpp knowledge_manager.cpp web_fetcher.cpp html_parser.cpp vector_index.cpp persistent_mixer.cpp compressed_knowledge.cpp block_access.cpp word_tokenizer.cpp word_ppm.cpp embedding_trainer.cpp rag_engine.cpp conversation_memory.cpp reasoning_engine.cpp bpe_tokenizer.cpp real_embeddings.cpp mini_transformer.cpp optimizer.cpp loss.cpp transformer_gradients.cpp test_block_access.cpp -lwinhttp -lws2_32 -pthread
cd ..
echo Build complete!
