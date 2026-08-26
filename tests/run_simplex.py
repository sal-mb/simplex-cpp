"""
Script 1: Executor de Experimentos do Algoritmo Simplex
------------------------------------------------------
Varia Perfis de Preprocessamento (5 perfis completos) combinados com Grid Search:
- Epsilon (--epsilon)
- Refactor Period (--refactor_period)
- Bland Threshold (--bland_threshold)

Faz o parsing das métricas estruturadas ([STATS]) da main.cpp e exporta para CSV.
"""

import os
import re
import itertools
import subprocess
import threading
from pathlib import Path
from datetime import datetime
from concurrent.futures import ThreadPoolExecutor, as_completed
import pandas as pd

# ==================== CONFIGURAÇÕES DE EXECUÇÃO ====================
EXECUTABLE_PATH = Path("../algorithm/.build/simplex")

# Pode ser um DIRETÓRIO ou um ARQUIVO INDIVIDUAL
INSTANCES_DIR = Path("../instances")

RESULTS_DIR = Path("results")
TIMEOUT_SECONDS = 3600  # Tempo limite por execução (segundos)

MAX_WORKERS = max((os.cpu_count() or 1) // 2, 1)

CLI_PARAM_MAP = {
    "scaling": {"flag": "--scaling", "is_bool": True},
    "remove_fixed": {"flag": "--remove-fixed", "is_bool": True},
    "remove_empty_rows": {"flag": "--remove-empty-rows", "is_bool": True},
    "remove_empty_cols": {"flag": "--remove-empty-cols", "is_bool": True},
    "remove_singleton_rows": {"flag": "--remove-singleton-rows", "is_bool": True},
    "remove_redundant_forcing": {"flag": "--remove-redundant-forcing", "is_bool": True},
    "tighten_bounds": {"flag": "--tighten-bounds", "is_bool": True},
    "seed": {"flag": "--seed", "is_bool": False},
    "epsilon": {"flag": "--epsilon", "is_bool": False},
    "refactor_period": {"flag": "--refactor_period", "is_bool": False},
    "bland_threshold": {"flag": "--bland_threshold", "is_bool": False},
    "verbose": {"flag": "--verbose", "is_bool": True},
}

# ==================== 1. PERFIS DE PREPROCESSAMENTO RESTAURADOS ====================
PREPROCESS_PROFILES = {
    "none": {
        "scaling": False,
        "remove_fixed": False,
        "remove_empty_rows": False,
        "remove_empty_cols": False,
        "remove_singleton_rows": False,
        "remove_redundant_forcing": False,
        "tighten_bounds": False,
    },
    "light": {
        "scaling": False,
        "remove_fixed": True,
        "remove_empty_rows": True,
        "remove_empty_cols": True,
        "remove_singleton_rows": False,
        "remove_redundant_forcing": False,
        "tighten_bounds": False,
    },
    "scaling_only": {
        "scaling": True,
        "remove_fixed": False,
        "remove_empty_rows": False,
        "remove_empty_cols": False,
        "remove_singleton_rows": False,
        "remove_redundant_forcing": False,
        "tighten_bounds": False,
    },
    "bound_reduction": {
        "scaling": False,
        "remove_fixed": True,
        "remove_empty_rows": True,
        "remove_empty_cols": True,
        "remove_singleton_rows": False,
        "remove_redundant_forcing": True,
        "tighten_bounds": True,
    },
    "full": {
        "scaling": True,
        "remove_fixed": True,
        "remove_empty_rows": True,
        "remove_empty_cols": True,
        "remove_singleton_rows": True,
        "remove_redundant_forcing": True,
        "tighten_bounds": True,
    },
}

RUN_PREP_PROFILES = ["none", "light", "full"]

# ==================== 2. VALORES NUMÉRICOS PARA VARIAR ====================
NUMERICAL_GRID = {
    "epsilon": [1e-12, 1e-11, 1e-10, 1e-9, 1e-8, 1e-7, 1e-6, 1e-5],
    "refactor_period": [1,20],
    "bland_threshold": [0.0, 1.0],
}

# Demais parâmetros estáticos
STATIC_PARAMS = {
    "seed": -1,
    "verbose": False,
}
# ====================================================================

print_lock = threading.Lock()
completed_count = 0


def get_dynamic_filename() -> str:
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    return f"grid_num_prep_{timestamp}.csv"


def parse_output(stdout_text: str, stderr_text: str, returncode: int):
    combined_output = (stdout_text + "\n" + stderr_text).upper()

    stats = {}
    for line in stdout_text.splitlines():
        if line.startswith("[STATS]"):
            parts = line.replace("[STATS]", "").strip().split(":", 1)
            if len(parts) == 2:
                key = parts[0].strip()
                val_str = parts[1].strip()
                try:
                    if "." in val_str or "e" in val_str.lower():
                        stats[key] = float(val_str)
                    else:
                        stats[key] = int(val_str)
                except ValueError:
                    stats[key] = val_str

    status = "SUCCESS" if returncode == 0 else "ERROR"

    if "INFEASIBLE" in combined_output:
        status = "INFEASIBLE"
    elif "UNBOUNDED" in combined_output:
        status = "UNBOUNDED"
    elif (
        "FACTORIZATION FAILED" in combined_output or "UMFPACK FAILED" in combined_output
    ):
        status = "ERROR"

    return stats, status


def run_single_experiment(task):
    global completed_count
    run_id, instance_path, profile_name, params, total_runs = task
    inst_name = instance_path.name

    cmd = [str(EXECUTABLE_PATH), str(instance_path)]

    for param_key, param_value in params.items():
        if param_key in CLI_PARAM_MAP:
            mapping = CLI_PARAM_MAP[param_key]
            if mapping["is_bool"]:
                if param_value:
                    cmd.append(mapping["flag"])
            else:
                cmd.extend([mapping["flag"], str(param_value)])

    try:
        process = subprocess.run(
            cmd, capture_output=True, text=True, timeout=TIMEOUT_SECONDS
        )
        stats, status = parse_output(
            process.stdout, process.stderr, process.returncode
        )
        stdout_output = process.stdout
    except subprocess.TimeoutExpired:
        stats, status = {}, "TIMEOUT"
        stdout_output = "TIMEOUT EXPIRED"
    except Exception as e:
        stats, status = {}, "ERROR"
        stdout_output = f"EXCEPTION: {type(e).__name__}"

    time_val = stats.get("Total_Time")
    cost_val = stats.get("Objective_Cost", status if status != "SUCCESS" else None)

    with print_lock:
        completed_count += 1
        time_disp = f"{time_val:.3f}s" if time_val is not None else "-"
        cfg_str = f"Prof:{profile_name:<15} | eps:{params['epsilon']} | ref:{params['refactor_period']} | bland:{params['bland_threshold']}"
        print(
            f"[{completed_count}/{total_runs}] {inst_name:<16} | {cfg_str} | Tempo: {time_disp:>8} | Status: {status}"
        )

    return {
        "Run_ID": run_id,
        "Instance": inst_name,
        "Profile": profile_name,
        "Scaling": params.get("scaling", False),
        "Remove_Fixed": params.get("remove_fixed", False),
        "Remove_Empty_Rows": params.get("remove_empty_rows", False),
        "Remove_Empty_Cols": params.get("remove_empty_cols", False),
        "Remove_Singleton_Rows": params.get("remove_singleton_rows", False),
        "Remove_Redundant_Forcing": params.get("remove_redundant_forcing", False),
        "Tighten_Bounds": params.get("tighten_bounds", False),
        "Seed": params.get("seed", -1),
        "Epsilon": params.get("epsilon", 1e-5),
        "Refactor_Period": params.get("refactor_period", 20),
        "Bland_Threshold": params.get("bland_threshold", 1.0),
        "Verbose": params.get("verbose", False),
        "Orig_Constraints": stats.get("Orig_Constraints"),
        "Orig_Variables": stats.get("Orig_Variables"),
        "Prep_Constraints": stats.get("Prep_Constraints"),
        "Prep_Variables": stats.get("Prep_Variables"),
        "Phase0_Iterations": stats.get("Phase0_Iterations"),
        "Phase0_Cost": stats.get("Phase0_Cost"),
        "Phase0_Time": stats.get("Phase0_Time"),
        "Phase1_Iterations": stats.get("Phase1_Iterations"),
        "Phase1_Cost": stats.get("Phase1_Cost"),
        "Objective_Cost": stats.get("Objective_Cost"),
        "Total_Time": time_val,
        "Time_Seconds": time_val,
        "Cost": cost_val,
        "Status": status,
        "Std_Output": stdout_output,
    }


def generate_task_combinations():
    num_keys = list(NUMERICAL_GRID.keys())
    num_values = list(NUMERICAL_GRID.values())
    grid_product = [dict(zip(num_keys, v)) for v in itertools.product(*num_values)]

    task_combinations = []
    for profile_name in RUN_PREP_PROFILES:
        if profile_name not in PREPROCESS_PROFILES:
            continue
        prep_params = PREPROCESS_PROFILES[profile_name]

        for num_params in grid_product:
            merged = {**prep_params, **num_params, **STATIC_PARAMS}
            task_combinations.append((profile_name, merged))

    return task_combinations


def run_benchmark():
    if not EXECUTABLE_PATH.exists():
        print(f"❌ Erro: Executável não encontrado em '{EXECUTABLE_PATH}'")
        return

    if not INSTANCES_DIR.exists():
        print(f"❌ Erro: Caminho de instâncias não encontrado em '{INSTANCES_DIR}'")
        return

    if INSTANCES_DIR.is_file():
        instance_files = [INSTANCES_DIR]
    elif INSTANCES_DIR.is_dir():
        instance_files = sorted([f for f in INSTANCES_DIR.glob("*") if f.is_file()])
    else:
        print(f"❌ Erro: Caminho '{INSTANCES_DIR}' inválido.")
        return

    if not instance_files:
        print(f"⚠️ Aviso: Nenhuma instância encontrada em '{INSTANCES_DIR}'")
        return

    RESULTS_DIR.mkdir(parents=True, exist_ok=True)
    output_filename = get_dynamic_filename()
    output_csv = RESULTS_DIR / output_filename

    task_combos = generate_task_combinations()
    total_runs = len(instance_files) * len(task_combos)

    print("=" * 80)
    print(" 🚀 INICIANDO EXPERIMENTO COMBINADO (PREP PROFILES + NUMERICAL GRID)")
    print(
        f" 📂 Instâncias: {len(instance_files)} | Perfis Prep: {len(RUN_PREP_PROFILES)} | Combinações Numéricas: {len(task_combos) // len(RUN_PREP_PROFILES)}"
    )
    print(f" 🔁 Total de Execuções: {total_runs} | Threads Ativas: {MAX_WORKERS}")
    print(f" 📁 Arquivo de Saída: {output_csv}")
    print("=" * 80 + "\n")

    tasks = []
    run_id = 0
    for instance in instance_files:
        for profile_name, params in task_combos:
            run_id += 1
            tasks.append((run_id, instance, profile_name, params, total_runs))

    results = []
    with ThreadPoolExecutor(max_workers=MAX_WORKERS) as executor:
        futures = [executor.submit(run_single_experiment, task) for task in tasks]
        for future in as_completed(futures):
            results.append(future.result())

    results.sort(key=lambda x: x["Run_ID"])

    df = pd.DataFrame(results)
    df.to_csv(output_csv, index=False)

    print("\n" + "=" * 80)
    print(" ✅ EXECUÇÃO FINALIZADA COM SUCESSO!")
    print(f"   • Dados brutos CSV: {output_csv}")
    print("=" * 80)


if __name__ == "__main__":
    run_benchmark()
