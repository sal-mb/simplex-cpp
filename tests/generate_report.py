"""
Script 2: Gerador de Relatório Executivo em Excel (.xlsx)
-------------------------------------------------------
Converte os resultados coletados no CSV em uma planilha Excel formatada
com KPIs, tabelas dinâmicas de desempenho e destaques operacionais.
"""

import sys
import pandas as pd
import openpyxl
from openpyxl.styles import Font, PatternFill, Alignment, Border, Side
from openpyxl.utils import get_column_letter

def generate_excel_report(csv_file="resultados_grid_search.csv", excel_file="Resultados_Simplex_Parametros.xlsx"):
    try:
        df = pd.read_csv(csv_file)
    except Exception as e:
        print(f"❌ Erro ao ler '{csv_file}': {e}")
        return

    wb = openpyxl.Workbook()
    wb.remove(wb.active)  # Remove aba padrão

    # Estilos Visuais
    header_fill = PatternFill(start_color="1F4E79", end_color="1F4E79", fill_type="solid")
    header_font = Font(name="Calibri", size=11, bold=True, color="FFFFFF")
    title_font = Font(name="Calibri", size=16, bold=True, color="1F4E79")
    bold_font = Font(name="Calibri", size=11, bold=True)
    regular_font = Font(name="Calibri", size=11)
    
    thin_border = Border(
        left=Side(style='thin', color='D9D9D9'),
        right=Side(style='thin', color='D9D9D9'),
        top=Side(style='thin', color='D9D9D9'),
        bottom=Side(style='thin', color='D9D9D9')
    )

    success_fill = PatternFill(start_color="E2EFDA", end_color="E2EFDA", fill_type="solid")
    error_fill = PatternFill(start_color="FCE4D6", end_color="FCE4D6", fill_type="solid")

    # ==================== ABA 1: RESUMO EXECUTIVO ====================
    ws_summary = wb.create_sheet(title="Resumo Executivo")
    ws_summary.views.sheetView[0].showGridLines = True

    ws_summary["A1"] = "Relatório de Desempenho - Solver Simplex"
    ws_summary["A1"].font = title_font

    total_runs = len(df)
    success_runs = len(df[df["Status"] == "SUCCESS"])
    timeout_runs = len(df[df["Status"] == "TIMEOUT"])
    infeasible_runs = len(df[df["Status"] == "INFEASIBLE"])
    error_runs = total_runs - success_runs - timeout_runs - infeasible_runs
    avg_time = df[df["Status"] == "SUCCESS"]["Time_Seconds"].mean()

    # Cards de KPI
    kpis = [
        ("Total de Testes", total_runs, "A3", "B3"),
        ("Sucessos", success_runs, "D3", "E3"),
        ("Inviáveis", infeasible_runs, "G3", "H3"),
        ("Timeouts", timeout_runs, "A6", "B6"),
        ("Erros/Falhas", error_runs, "D6", "E6"),
        ("Tempo Médio (Sucesso)", f"{avg_time:.4f}s" if pd.notnull(avg_time) else "-", "G6", "H6")
    ]

    for title, val, cell_t, cell_v in kpis:
        ws_summary[cell_t] = title
        ws_summary[cell_t].font = Font(name="Calibri", size=10, color="595959")
        ws_summary[cell_v] = val
        ws_summary[cell_v].font = Font(name="Calibri", size=14, bold=True, color="1F4E79")

    # ==================== ABA 2: DADOS COMPLETOS ====================
    ws_data = wb.create_sheet(title="Resultados Grid Search")
    ws_data.views.sheetView[0].showGridLines = True

    headers = list(df.columns)
    ws_data.append(headers)

    for col_idx, h in enumerate(headers, 1):
        cell = ws_data.cell(row=1, column=col_idx)
        cell.fill = header_fill
        cell.font = header_font
        cell.alignment = Alignment(horizontal="center", vertical="center")

    for row_idx, row_data in enumerate(df.values, 2):
        ws_data.append(list(row_data))
        status = row_data[-1]
        for col_idx in range(1, len(headers) + 1):
            cell = ws_data.cell(row=row_idx, column=col_idx)
            cell.font = regular_font
            cell.border = thin_border
            if status == "SUCCESS":
                cell.fill = success_fill
            elif status in ["ERROR", "TIMEOUT", "INFEASIBLE", "UNBOUNDED"]:
                cell.fill = error_fill

    # ==================== ABA 3: ANÁLISE POR PARÂMETRO ====================
    ws_params = wb.create_sheet(title="Análise por Parâmetro")
    ws_params.views.sheetView[0].showGridLines = True

    ws_params["A1"] = "Impacto dos Parâmetros no Tempo de Execução (Apenas Casos de Sucesso)"
    ws_params["A1"].font = title_font

    current_row = 3
    df_success = df[df["Status"] == "SUCCESS"]

    for param in ["Preprocess", "Seed", "Epsilon", "Refactor_Period"]:
        ws_params.cell(row=current_row, column=1, value=f"Parâmetro: {param}").font = bold_font
        current_row += 1
        
        grouped = df_success.groupby(param)["Time_Seconds"].agg(["count", "mean", "min", "max"]).reset_index()
        grouped.columns = [param, "Qtd Sucessos", "Tempo Médio (s)", "Mínimo (s)", "Máximo (s)"]

        # Cabeçalhos da tabela
        for c_idx, col_name in enumerate(grouped.columns, 1):
            cell = ws_params.cell(row=current_row, column=c_idx, value=col_name)
            cell.fill = header_fill
            cell.font = header_font
            cell.alignment = Alignment(horizontal="center")
        
        current_row += 1

        # Linhas da tabela
        for r_vals in grouped.values:
            for c_idx, val in enumerate(r_vals, 1):
                cell = ws_params.cell(row=current_row, column=c_idx)
                if c_idx > 2:
                    cell.value = round(val, 6) if pd.notnull(val) else "-"
                else:
                    cell.value = val
                cell.font = regular_font
                cell.border = thin_border
            current_row += 1
        
        current_row += 2

    # Auto-ajuste de largura de colunas em todas as abas
    for sheet in wb.worksheets:
        for col in sheet.columns:
            max_len = max(len(str(cell.value or '')) for cell in col)
            col_letter = get_column_letter(col[0].column)
            sheet.column_dimensions[col_letter].width = max(max_len + 3, 12)

    wb.save(excel_file)
    print(f"📊 Relatório Excel gerado com sucesso: '{excel_file}'")

if __name__ == "__main__":
    generate_excel_report()
