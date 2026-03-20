# server/lora_collector.py
import json
import subprocess
from pathlib import Path
from symbol_graph import SymbolGraph

class LoraCollector:
    def __init__(self, workspace_root: str):
        self.workspace_root = Path(workspace_root).resolve()
        self.engine_path = str(self.workspace_root / "bin" / "neural_engine.exe")

    def create_sft_dataset(self, output_path="brain/training/sft_dataset.json"):
        """
        Turns your code into a supervised fine-tuning dataset.
        Format: {"prompt": "Write a function for...", "completion": "<code>"}
        """
        print(f"\n[LoRA] Collecting style samples from your codebase...")
        
        graph = SymbolGraph(str(self.workspace_root))
        data = graph._graph()
        
        dataset = []
        
        # We'll take functions/methods as training samples
        for name, defs in data["definitions"].items():
            for d in defs:
                if d["kind"] not in ["function", "method"]: continue
                
                path = d["path"]
                full_path = self.workspace_root / path
                if not full_path.exists(): continue
                
                lines = full_path.read_text(encoding="utf-8").splitlines()
                code = "\n".join(lines[d["line_start"]-1 : d["line_end"]])
                
                # Create a prompt-completion pair
                # We'll use the docstring or just the signature as the prompt
                prompt = f"Implement a {d['kind']} named '{name}' in {path}."
                
                dataset.append({
                    "instruction": prompt,
                    "input": f"File: {path}, Symbol: {name}",
                    "output": code
                })
        
        # Save to JSON
        dataset_file = self.workspace_root / output_path
        dataset_file.parent.mkdir(parents=True, exist_ok=True)
        dataset_file.write_text(json.dumps(dataset, indent=2), encoding="utf-8")
        
        print(f"[OK] Generated {len(dataset)} training samples at {output_path}")
        return str(dataset_file)

    def run_fine_tuning(self, dataset_path):
        """Triggers the C++ SFT pipeline."""
        print(f"\n[LoRA] Starting C++ Supervised Fine-Tuning (SFT)...")
        print(f"      This will specialize the MiniTransformer to your coding style.")
        
        try:
            # Command: neural_engine sft <dataset_path> <epochs> <lr>
            # Based on neural_engine.cpp:2436
            result = subprocess.run(
                [self.engine_path, "sft", dataset_path, "3", "0.0001"],
                cwd=str(self.workspace_root),
                capture_output=True,
                text=True
            )
            print(result.stdout)
            if result.returncode == 0:
                print(f"[OK] LoRA Fine-tuning complete! Model weights updated.")
            else:
                print(f"[FAIL] Fine-tuning error: {result.stderr}")
        except Exception as e:
            print(f"[ERROR] Could not run LoRA pipeline: {str(e)}")

if __name__ == "__main__":
    collector = LoraCollector(".")
    data_path = collector.create_sft_dataset()
    collector.run_fine_tuning(data_path)
