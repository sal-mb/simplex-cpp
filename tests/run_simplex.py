import os
import re
import subprocess
from pathlib import Path
import pandas as pd

# ==================== CONFIGURAÇÕES ====================
EXECUTABLE_PATH = Path("../algorithm/.build/simplex")
INSTANCES_DIR = Path("../instances_working")
OUTPUT_CSV = "resultados_simplex.csv"
OUTPUT_MD = "relatorio_simplex.md"
OUTPUT_HTML = "relatorio_simplex.html"
TIMEOUT_SECONDS = 120  # Tempo máximo em segundos por instância
# =======================================================

def parse_output(stdout_text):
    """Extrai o custo objetivo e o tempo total da saída do terminal."""
    time_val = "-"
    cost_val = "-"

    time_match = re.search(r"Total time:\s*([\d\.\-eE]+)\s*seconds", stdout_text, re.IGNORECASE)
    if time_match:
        try:
            time_val = f"{float(time_match.group(1)):.3f}"
        except ValueError:
            time_val = time_match.group(1)

    cost_match = re.search(r"Objective cost:\s*([\d\.\-eE]+)", stdout_text, re.IGNORECASE)
    if cost_match:
        try:
            cost_val = f"{float(cost_match.group(1)):.2f}"
        except ValueError:
            cost_val = cost_match.group(1)

    if "INFEASIBLE" in stdout_text.upper():
        cost_val = "INFEASIBLE"
    elif "UNBOUNDED" in stdout_text.upper():
        cost_val = "UNBOUNDED"

    return time_val, cost_val

def generate_markdown_report(df, filename):
    """Gera um relatório legível em Markdown com estatísticas e tabela."""
    total = len(df)
    erros = len(df[df['Custo'] == 'ERRO'])
    timeouts = len(df[df['Custo'] == 'TIMEOUT'])
    infeasible = len(df[df['Custo'] == 'INFEASIBLE'])
    solv = total - (erros + timeouts + infeasible)

    df_solved = df[~df['Tempo (s)'].isin(['-', 'TIMEOUT', 'ERRO'])].copy()
    df_solved['Tempo_num'] = pd.to_numeric(df_solved['Tempo (s)'], errors='coerce')
    avg_time = df_solved['Tempo_num'].mean() if not df_solved.empty else 0

    md_content = f"""# 📊 Relatório de Execução — Algoritmo Simplex

## 📈 Resumo Executivo
- **Total de Instâncias Processadas**: `{total}`
- **Resolvidas com Sucesso**: `{solv}`
- **Inviáveis (Infeasible)**: `{infeasible}`
- **Erros de Execução**: `{erros}`
- **Timeouts**: `{timeouts}`
- **Tempo Médio de Execução**: `{avg_time:.4f}s`

---

## 📋 Tabela Completa de Resultados

| Instância | Tempo (s) | Custo / Status |
| :--- | :---: | :---: |
"""
    for _, row in df.iterrows():
        status_icon = "✅" if row['Custo'] not in ['ERRO', 'TIMEOUT', 'INFEASIBLE', '-'] else "❌"
        if row['Custo'] == 'INFEASIBLE':
            status_icon = "⚠️"
        elif row['Custo'] == 'TIMEOUT':
            status_icon = "⏳"
            
        md_content += f"| `{row['Instância']}` | {row['Tempo (s)']} | {status_icon} {row['Custo']} |\n"

    with open(filename, "w", encoding="utf-8") as f:
        f.write(md_content)

def run_instances():
    if not EXECUTABLE_PATH.exists():
        print(f"Erro: Executável não encontrado em '{EXECUTABLE_PATH}'")
        return

    if not INSTANCES_DIR.exists():
        print(f"Erro: Diretório de instâncias não encontrado em '{INSTANCES_DIR}'")
        return

    instance_files = sorted([f for f in INSTANCES_DIR.glob("*") if f.is_file()])
    total_files = len(instance_files)
    results = []

    print("=" * 60)
    print(" 🚀 INICIANDO EXECUÇÃO DAS INSTÂNCIAS DO SIMPLEX")
    print("=" * 60 + "\n")

    for idx, file_path in enumerate(instance_files, start=1):
        instance_name = file_path.name

        # Imprime a instância ATUAL antes de rodar o subprocess
        print(f"[{idx}/{total_files}] Executando: {instance_name:<20} ... ", end="", flush=True)

        try:
            process = subprocess.run(
                [str(EXECUTABLE_PATH), str(file_path)],
                capture_output=True,
                text=True,
                timeout=TIMEOUT_SECONDS
            )

            if process.returncode != 0:
                time_val, cost_val = "-", "ERRO"
            else:
                time_val, cost_val = parse_output(process.stdout)

        except subprocess.TimeoutExpired:
            time_val, cost_val = "TIMEOUT", "-"
        except Exception:
            time_val, cost_val = "-", "ERRO"

        # Conclui a linha com o resultado obtido
        print(f"Tempo: {time_val:>7}s | Custo: {cost_val}")

        results.append({
            "Instância": instance_name,
            "Tempo (s)": time_val,
            "Custo": cost_val
        })

    df = pd.DataFrame(results)

    # 1. Salvar CSV
    df.to_csv(OUTPUT_CSV, index=False)

    # 2. Gerar Relatório Markdown Human-Readable
    generate_markdown_report(df, OUTPUT_MD)

    # 3. Exibir Resumo Amigável no Terminal
    print("\n" + "=" * 60)
    print(" 📌 TABELA FINAL DE RESULTADOS")
    print("=" * 60)
    print(df.to_string(index=False))
    print("=" * 60)
    print(f"\n Arquivos gerados com sucesso:")
    print(f"  • Dados brutos: {OUTPUT_CSV}")
    print(f"  • Relatório Markdown: {OUTPUT_MD}")

if __name__ == "__main__":
    run_instances()
