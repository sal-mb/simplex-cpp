"""
Script 1: Executor de Experimentos do Algoritmo Simplex (Grid Search) - Multithreaded
-------------------------------------------------------------------------------------
Varia os parâmetros da CLI C++ em paralelo e registra o tempo de execução, custo, status e stdout.
Suporta passagem de diretório ou de uma única instância individual.
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

# Pode ser um DIRETÓRIO (ex: Path("../instances/tuff.mps"))
# ou um ARQUIVO INDIVIDUAL (ex: Path("../instances/tuff.mps/instance1.mps"))
INSTANCES_DIR = Path("../instances/tuff.mps")

RESULTS_DIR = Path("results")
TIMEOUT_SECONDS = 1800  # Tempo limite por execução (segundos)

# Utiliza metade dos núcleos de CPU do sistema (com limite mínimo de 1)
MAX_WORKERS = max((os.cpu_count() or 1) // 2, 1)

# Mapeamento dos parâmetros para as flags em formato longo (Full Flags) da CLI C++ e abreviações no nome do arquivo
CLI_PARAM_MAP = {
    "scaling": {"flag": "--scaling", "is_bool": True, "abbr": "scl"},
    "remove_fixed": {"flag": "--remove-fixed", "is_bool": True, "abbr": "rmf"},
    "seed": {"flag": "--seed", "is_bool": False, "abbr": "sd"},
    "epsilon": {"flag": "--epsilon", "is_bool": False, "abbr": "eps"},
    "refactor_period": {"flag": "--refactor_period", "is_bool": False, "abbr": "ref"},
    "verbose": {"flag": "--verbose", "is_bool": True, "abbr": "v"},
}

# GRID DE PARÂMETROS PARA TESTAR
PARAM_GRID = {
    "scaling": [False, True],  # --scaling
    "remove_fixed": [False, True],  # --remove-fixed
    "seed": [-1],  # --seed
    "epsilon": [  # --epsilon
        1e-10,
        1e-9,
        1e-8,
        1e-7,
        1e-6,
    ],
    "refactor_period": [1, 20],  # --refactor_period
    "verbose": [False],  # --verbose
}
# ====================================================================

print_lock = threading.Lock()
completed_count = 0


def get_dynamic_filename() -> str:
    """Gera o nome do arquivo CSV incluindo apenas as chaves dos parâmetros que variam."""
    varying_abbrs = [
        CLI_PARAM_MAP[key]["abbr"]
        for key, values in PARAM_GRID.items()
        if len(values) > 1 and key in CLI_PARAM_MAP
    ]

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")

    if varying_abbrs:
        prefix = "_".join(varying_abbrs)
        return f"{prefix}_{timestamp}.csv"
    return f"simplex_{timestamp}.csv"


def parse_output(stdout_text: str, stderr_text: str, returncode: int):
    """Extrai tempo, custo e status combinando stdout e exceções do C++."""
    combined_output = (stdout_text + "\n" + stderr_text).upper()

    time_val = None
    cost_val = None
    status = "SUCCESS" if returncode == 0 else "ERROR"

    # Captura tempo no formato: "Total time: X.XXXXXX seconds" (main.cpp)
    time_match = re.search(
        r"Total time:\s*([\d\.\-eE]+)\s*seconds", stdout_text, re.IGNORECASE
    )
    if time_match:
        try:
            time_val = float(time_match.group(1))
        except ValueError:
            time_val = None

    # Captura custo no formato: "Objective cost: X.XXXXXX" (main.cpp)
    cost_match = re.search(
        r"Objective cost:\s*([\d\.\-eE]+)", stdout_text, re.IGNORECASE
    )
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
    elif (
        "FACTORIZATION FAILED" in combined_output or "UMFPACK FAILED" in combined_output
    ):
        cost_val = "FACTORIZATION_ERROR"
        status = "ERROR"

    return time_val, cost_val, status


def run_single_experiment(task):
    global completed_count
    run_id, instance_path, params, total_runs = task
    inst_name = instance_path.name

    cmd = [str(EXECUTABLE_PATH), str(instance_path)]

    # Constrói o comando dinamicamente usando flags completas
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
        time_val, cost_val, status = parse_output(
            process.stdout, process.stderr, process.returncode
        )
        stdout_output = process.stdout
    except subprocess.TimeoutExpired:
        time_val, cost_val, status = None, "TIMEOUT", "TIMEOUT"
        stdout_output = "TIMEOUT EXPIRED"
    except Exception as e:
        time_val, cost_val, status = None, f"ERRO ({type(e).__name__})", "ERROR"
        stdout_output = f"EXCEPTION: {type(e).__name__}"

    with print_lock:
        completed_count += 1
        param_str = " | ".join(
            f"{CLI_PARAM_MAP[k]['abbr'].upper()}={v}"
            for k, v in params.items()
            if k in CLI_PARAM_MAP
        )
        time_disp = f"{time_val:.3f}s" if time_val is not None else "-"
        print(
            f"[{completed_count}/{total_runs}] {inst_name:<18} | {param_str:<60} | Tempo: {time_disp:>8} | Status: {status}"
        )

    return {
        "Run_ID": run_id,
        "Instance": inst_name,
        "Scaling": params.get("scaling", False),
        "Remove_Fixed": params.get("remove_fixed", False),
        "Seed": params.get("seed", -1),
        "Epsilon": params.get("epsilon", 1e-5),
        "Refactor_Period": params.get("refactor_period", 20),
        "Verbose": params.get("verbose", False),
        "Time_Seconds": time_val,
        "Cost": cost_val,
        "Status": status,
        "Std_Output": stdout_output,
    }


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
        print(f"❌ Erro: Caminho de instâncias não encontrado em '{INSTANCES_DIR}'")
        return

    # Suporta arquivo único ou diretório
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

    combinations = generate_parameter_combinations()
    total_runs = len(instance_files) * len(combinations)

    print("=" * 80)
    print(" 🚀 INICIANDO GRID SEARCH PARALELO DO SIMPLEX")
    print(
        f" 📂 Instâncias: {len(instance_files)} | Combinações de Parâmetros: {len(combinations)}"
    )
    print(f" 🔁 Total de Execuções: {total_runs} | Threads Ativas: {MAX_WORKERS}")
    print(f" 📁 Arquivo de Saída: {output_csv}")
    print("=" * 80 + "\n")

    tasks = []
    run_id = 0
    for instance in instance_files:
        for params in combinations:
            run_id += 1
            tasks.append((run_id, instance, params, total_runs))

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
    run_grid_search()
