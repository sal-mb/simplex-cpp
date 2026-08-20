"""
Script 1: Executor de Experimentos do Algoritmo Simplex (Grid Search)
-------------------------------------------------------------------
Varia os parâmetros da CLI C++ e registra o tempo de execução, custo e status.
"""

import os
import re
import itertools
import subprocess
from pathlib import Path
import pandas as pd
import json

# ==================== CONFIGURAÇÕES DE EXECUÇÃO ====================
EXECUTABLE_PATH = Path("../algorithm/.build/simplex")
INSTANCES_DIR = Path("../instances")
OUTPUT_CSV = "resultados_grid_search.csv"
OUTPUT_JSON = "resultados_grid_search.json"
TIMEOUT_SECONDS = 1800  # Tempo limite por execução (segundos)

# GRID DE PARÂMETROS PARA TESTAR
PARAM_GRID = {
    "preprocess": [False, True],            # -p / --preprocess
    "seed": [-1],                       # -s / --seed
    "epsilon": [1e-11, 1e-10, 1e-9, 1e-8,1e-7,1e-6, 1e-5, 1e-4, 1e-3],          # -e / --epsilon
    "refactor_period": [1, 3, 5, 10, 20, 50]         # -r / --refactor_period
}
# ====================================================================

def parse_output(stdout_text: str, stderr_text: str, returncode: int):
    """Extrai tempo, custo e status combinando stdout e exceções do C++."""
    combined_output = (stdout_text + "\n" + stderr_text).upper()
    
    time_val = None
    cost_val = None
    status = "SUCCESS" if returncode == 0 else "ERROR"

    # Captura tempo no formato: "Total time: X.XXXXXX seconds" (main.cpp)
    time_match = re.search(r"Total time:\s*([\d\.\-eE]+)\s*seconds", stdout_text, re.IGNORECASE)
    if time_match:
        try:
            time_val = float(time_match.group(1))
        except ValueError:
            time_val = None

    # Captura custo no formato: "Objective cost: X.XXXXXX" (main.cpp)
    cost_match = re.search(r"Objective cost:\s*([\d\.\-eE]+)", stdout_text, re.IGNORECASE)
    if cost_match:
        try:
            cost_val = float(cost_match.group(1))
        except ValueError:
            cost_val = cost_match.group(1)

    # Detecção de exceções lançadas em simplex.cpp
    if "INFEASIBLE" in combined_output:
        cost_val = "INFEASIBLE"
        status = "INFEASIBLE"
    elif "UNBOUNDED" in combined_output:
        cost_val = "UNBOUNDED"
        status = "UNBOUNDED"
    elif "FACTORIZATION FAILED" in combined_output or "UMFPACK FAILED" in combined_output:
        cost_val = "FACTORIZATION_ERROR"
        status = "ERROR"

    return time_val, cost_val, status

def generate_parameter_combinations():
    """Gera o produto cartesiano das combinações de parâmetros."""
    keys = PARAM_GRID.keys()
    values = PARAM_GRID.values()
    return [dict(zip(keys, v)) for v in itertools.product(*values)]

def run_grid_search():
    if not EXECUTABLE_PATH.exists():
        print(f"❌ Erro: Executável não encontrado em '{EXECUTABLE_PATH}'")
        return

    if not INSTANCES_DIR.exists():
        print(f"❌ Erro: Diretório de instâncias não encontrado em '{INSTANCES_DIR}'")
        return

    instance_files = sorted([f for f in INSTANCES_DIR.glob("*") if f.is_file()])
    combinations = generate_parameter_combinations()
    total_runs = len(instance_files) * len(combinations)
    
    print("=" * 70)
    print(" 🚀 INICIANDO GRID SEARCH DO SIMPLEX")
    print(f" 📂 Instâncias: {len(instance_files)} | Combinações de Parâmetros: {len(combinations)}")
    print(f" 🔁 Total de Execuções Planejadas: {total_runs}")
    print("=" * 70 + "\n")

    results = []
    run_count = 0

    for instance in instance_files:
        for params in combinations:
            run_count += 1
            inst_name = instance.name
            
            # Monta comando CLI conforme params.hpp
            cmd = [str(EXECUTABLE_PATH), str(instance)]
            if params["preprocess"]:
                cmd.append("-p")
            cmd.extend(["-s", str(params["seed"])])
            cmd.extend(["-e", str(params["epsilon"])])
            cmd.extend(["-r", str(params["refactor_period"])])

            param_str = f"P={params['preprocess']} | S={params['seed']} | E={params['epsilon']} | R={params['refactor_period']}"
            print(f"[{run_count}/{total_runs}] {inst_name:<18} | {param_str:<45} ... ", end="", flush=True)

            try:
                process = subprocess.run(
                    cmd,
                    capture_output=True,
                    text=True,
                    timeout=TIMEOUT_SECONDS
                )

                time_val, cost_val, status = parse_output(process.stdout, process.stderr, process.returncode)

            except subprocess.TimeoutExpired:
                time_val, cost_val, status = None, "TIMEOUT", "TIMEOUT"
            except Exception as e:
                time_val, cost_val, status = None, f"ERRO ({type(e).__name__})", "ERROR"

            time_disp = f"{time_val:.3f}s" if time_val is not None else "-"
            print(f"Tempo: {time_disp:>8} | Status: {status}")

            results.append({
                "Run_ID": run_count,
                "Instance": inst_name,
                "Preprocess": params["preprocess"],
                "Seed": params["seed"],
                "Epsilon": params["epsilon"],
                "Refactor_Period": params["refactor_period"],
                "Time_Seconds": time_val,
                "Cost": cost_val,
                "Status": status
            })

    # Salva resultados
    df = pd.DataFrame(results)
    df.to_csv(OUTPUT_CSV, index=False)
    with open(OUTPUT_JSON, "w", encoding="utf-8") as f:
        json.dump(results, f, indent=2)

    print("\n" + "=" * 70)
    print(" ✅ EXECUÇÃO FINALIZADA COM SUCESSO!")
    print(f"  • Dados brutos CSV: {OUTPUT_CSV}")
    print(f"  • Dados JSON: {OUTPUT_JSON}")
    print("=" * 70)

if __name__ == "__main__":
    run_grid_search()
