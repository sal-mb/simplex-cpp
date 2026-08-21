import re
import openpyxl
import pandas as pd
import numpy as np
from openpyxl.styles import Font, PatternFill, Alignment, Border, Side
from openpyxl.utils import get_column_letter

NETLIB_REFERENCE_DATA = """
25FV47      822   1571    11127      70477        5.5018458883E+03
80BAU3B    2263   9799    29063     298952  B     9.8723216072E+05
ADLITTLE     57     97      465       3690        2.2549496316E+05
AFIRO        28     32       88        794       -4.6475314286E+02
AGG         489    163     2541      21865       -3.5991767287E+07
AGG2        517    302     4515      32552       -2.0239252356E+07
AGG3        517    302     4531      32570        1.0312115935E+07
BANDM       306    472     2659      19460       -1.5862801845E+02
BEACONFD    174    262     3476      17475        3.3592485807E+04
BLEND        75     83      521       3227       -3.0812149846E+01
BNL1        644   1175     6129      42473        1.9776292856E+03
BNL2       2325   3489    16124     127145        1.8112365404E+03
BOEING1     351    384     3865      25315  BR   -3.3521356751E+02
BOEING2     167    143     1339       8761  BR   -3.1501872802E+02
BORE3D      234    315     1525      13160  B     1.3730803942E+03
BRANDY      221    249     2150      14028        1.5185098965E+03
CAPRI       272    353     1786      15267  B     2.6900129138E+03
CYCLE      1904   2857    21322     166648  B    -5.2263930249E+00
CZPROB      930   3523    14173      92202  B     2.1851966989E+06
D2Q06C     2172   5167    35674     258038        1.2278423615E+05
D6CUBE      416   6184    43888     167633  B     3.1549166667E+02
DEGEN2      445    534     4449      24657       -1.4351780000E+03
DEGEN3     1504   1818    26230     130252       -9.8729400000E+02
DFL001     6072  12230    41873     353192  B     1.12664E+07
E226        224    282     2767      17749       -1.8751929066E+01
ETAMACRO    401    688     2489      21915  B    -7.5571521774E+02
FFFFF800    525    854     6235      39637        5.5567961165E+05
FINNIS      498    614     2714      23847  B     1.7279096547E+05
FIT1D        25   1026    14430      51734  B    -9.1463780924E+03
FIT1P       628   1677    10894      65116  B     9.1463780924E+03
FIT2D        26  10500   138018     482330  B    -6.8464293294E+04
FIT2P      3001  13525    60784     439794  B     6.8464293232E+04
FORPLAN     162    421     4916      25100  BR   -6.6421873953E+02
GANGES     1310   1681     7021      60191  B    -1.0958636356E+05
GFRD-PNC    617   1092     3467      24476  B     6.9022359995E+06
GREENBEA   2393   5405    31499     235711  B    -7.2462405908E+07
GREENBEB   2393   5405    31499     235739  B    -4.3021476065E+06
GROW15      301    645     5665      35041  B    -1.0687094129E+08
GROW22      441    946     8318      50789  B    -1.6083433648E+08
GROW7       141    301     2633      17043  B    -4.7787811815E+07
ISRAEL      175    142     2358      12109       -8.9664482186E+05
KB2          44     41      291       2526  B    -1.7499001299E+03
LOTFI       154    308     1086       6718       -2.5264706062E+01
MAROS       847   1443    10006      65906  B    -5.8063743701E+04
MAROS-R7   3137   9408   151120    4812587        1.4971851665E+06
MODSZK1     688   1620     4158      40908  B     3.2061972906E+02
NESM        663   2923    13988     117828  BR    1.4076073035E+07
PEROLD      626   1376     6026      47486  B    -9.3807580773E+03
PILOT      1442   3652    43220     278593  B    -5.5740430007E+02
PILOT.JA    941   1988    14706      97258  B    -6.1131344111E+03
PILOT.WE    723   2789     9218      79972  B    -2.7201027439E+06
PILOT4      411   1000     5145      40936  B    -2.5811392641E+03
PILOT87    2031   4883    73804     514192  B     3.0171072827E+02
PILOTNOV    976   2172    13129      89779  B    -4.4972761882E+03
QAP8        913   1632     8304                   2.0350000000E+02
QAP12      3193   8856    44244                   5.2289435056E+02
QAP15      6331  22275   110700                   1.0409940410E+03
RECIPE       92    180      752       6210  B    -2.6661600000E+02
SC105       106    103      281       3307       -5.2202061212E+01
SC205       206    203      552       6380       -5.2202061212E+01
SC50A        51     48      131       1615       -6.4575077059E+01
SC50B        51     48      119       1567       -7.0000000000E+01
SCAGR25     472    500     2029      17406       -1.4753433061E+07
SCAGR7      130    140      553       4953       -2.3313892548E+06
SCFXM1      331    457     2612      19078        1.8416759028E+04
SCFXM2      661    914     5229      37079        3.6660261565E+04
SCFXM3      991   1371     7846      53828        5.4901254550E+04
SCORPION    389    358     1708      12186        1.8781248227E+03
SCRS8       491   1169     4029      36760        9.0429998619E+02
SCSD1        78    760     3148      17852        8.6666666743E+00
SCSD6       148   1350     5666      32161        5.0500000078E+01
SCSD8       398   2750    11334      65888        9.0499999993E+02
SCTAP1      301    480     2052      14970        1.4122500000E+03
SCTAP2     1091   1880     8124      57479        1.7248071429E+03
SCTAP3     1481   2480    10734      78688        1.4240000000E+03
SEBA        516   1028     4874      38627  BR    1.5711600000E+04
SHARE1B     118    225     1182       8380       -7.6589318579E+04
SHARE2B      97     79      730       4795       -4.1573224074E+02
SHELL       537   1775     4900      38049  B     1.2088253460E+09
SHIP04L     403   2118     8450      57203        1.7933245380E+06
SHIP04S     403   1458     5810      41257        1.7987147004E+06
SHIP08L     779   4283    17085     117083        1.9090552114E+06
SHIP08S     779   2387     9501      70093        1.9200982105E+06
SHIP12L    1152   5427    21597     146753        1.4701879193E+06
SHIP12S    1152   2763    10941      82527        1.4892361344E+06
SIERRA     1228   2036     9252      76627  B     1.5394362184E+07
STAIR       357    467     3857      27405  B    -2.5126695119E+02
STANDATA    360   1075     3038      26135  B     1.2576995000E+03
STANDMPS    468   1075     3686      29839  B     1.4060175000E+03
STOCFOR1    118    111      474       4247       -4.1131976219E+04
STOCFOR2   2158   2031     9492      79845       -3.9024408538E+04
STOCFOR3  16676  15695    74004                  -3.9976661576E+04
TRUSS      1001   8806    36642                   4.5881584719E+05
TUFF        334    587     4523      29439  B     2.9214776509E-01
VTP.BASE    199    203      914       8175  B     1.2983146246E+05
WOOD1P      245   2594    70216     328905        1.4429024116E+00
WOODW      1099   8405    37478     240063        1.3044763331E+00
"""

def parse_netlib_reference(raw_text):
    ref_map = {}
    for line in raw_text.strip().split("\n"):
        tokens = line.strip().split()
        if not tokens:
            continue
        name = tokens[0].upper()
        opt_str = tokens[-1].replace("**", "")
        try:
            ref_map[name] = float(opt_str)
        except ValueError:
            ref_map[name] = None
    return ref_map

def normalize_inst_name(name):
    clean = str(name).upper()
    clean = re.sub(r'\.(MPS|QPS|SIF)$', '', clean)
    if clean == 'VTPBASE':
        clean = 'VTP.BASE'
    return clean

def fmt_sci(val):
    if pd.isna(val) or val is None:
        return "-"
    try:
        val = float(val)
        return f"{val:.1e}"
    except (ValueError, TypeError):
        return str(val)

def generate_excel_report(csv_file="resultados_grid_search.csv", excel_file="Resultados_Simplex_Parametros.xlsx"):
    try:
        df = pd.read_csv(csv_file)
    except Exception as e:
        print(f"❌ Erro ao ler '{csv_file}': {e}")
        return

    netlib_ref = parse_netlib_reference(NETLIB_REFERENCE_DATA)

    df["Norm_Instance"] = df["Instance"].apply(normalize_inst_name)
    df["Optimal_Ref"] = df["Norm_Instance"].map(netlib_ref)
    df["Cost_Num"] = pd.to_numeric(df["Cost"], errors='coerce')
    df["Time_Seconds"] = pd.to_numeric(df["Time_Seconds"], errors='coerce')
    df["Epsilon_Num"] = pd.to_numeric(df["Epsilon"], errors='coerce')

    df["Abs_Diff"] = (df["Cost_Num"] - df["Optimal_Ref"]).abs()
    df["Rel_Diff"] = np.where(
        df["Optimal_Ref"].abs() > 1e-9,
        df["Abs_Diff"] / df["Optimal_Ref"].abs(),
        df["Abs_Diff"]
    )
    
    df["Is_Optimal"] = (df["Status"] == "SUCCESS") & (df["Rel_Diff"] < 1e-4)

    wb = openpyxl.Workbook()
    wb.remove(wb.active)

    # Estilos de células
    header_fill = PatternFill(start_color="1F4E79", end_color="1F4E79", fill_type="solid")
    header_font = Font(name="Calibri", size=11, bold=True, color="FFFFFF")
    title_font = Font(name="Calibri", size=16, bold=True, color="1F4E79")
    subtitle_font = Font(name="Calibri", size=13, bold=True, color="1F4E79")
    regular_font = Font(name="Calibri", size=11)
    
    thin_border = Border(
        left=Side(style='thin', color='D9D9D9'),
        right=Side(style='thin', color='D9D9D9'),
        top=Side(style='thin', color='D9D9D9'),
        bottom=Side(style='thin', color='D9D9D9')
    )

    success_fill = PatternFill(start_color="E2EFDA", end_color="E2EFDA", fill_type="solid")
    warning_fill = PatternFill(start_color="FFF2CC", end_color="FFF2CC", fill_type="solid")
    error_fill = PatternFill(start_color="FCE4D6", end_color="FCE4D6", fill_type="solid")

    df_success = df[df["Status"] == "SUCCESS"].copy()

    # ==================== ABA 1: RESUMO EXECUTIVO ====================
    ws_summary = wb.create_sheet(title="Resumo Executivo")
    ws_summary.views.sheetView[0].showGridLines = True

    ws_summary["A1"] = "Relatório Executivo & Validação Netlib - Solver Simplex"
    ws_summary["A1"].font = title_font

    total_runs = len(df)
    success_runs = len(df_success)
    optimal_runs = df["Is_Optimal"].sum()
    divergent_runs = success_runs - optimal_runs
    timeout_runs = len(df[df["Status"] == "TIMEOUT"])
    infeasible_runs = len(df[df["Status"] == "INFEASIBLE"])
    error_runs = total_runs - success_runs - timeout_runs - infeasible_runs
    avg_time = df_success["Time_Seconds"].mean()
    acc_rate = (optimal_runs / success_runs) if success_runs > 0 else 0

    kpis = [
        ("Total de Testes", total_runs, "A3", "B3"),
        ("Sucessos (Solver)", success_runs, "D3", "E3"),
        ("Ótimos Confirmados (Netlib)", optimal_runs, "G3", "H3"),
        ("Soluções Divergentes", divergent_runs, "A6", "B6"),
        ("Timeouts", timeout_runs, "D6", "E6"),
        ("Inviáveis / Erros", infeasible_runs + error_runs, "G6", "H6"),
        ("Precisão das Soluções", f"{acc_rate:.1%}", "A9", "B9"),
        ("Tempo Médio (Sucessos)", f"{avg_time:.4f}s" if pd.notnull(avg_time) else "-", "D9", "E9")
    ]

    for title, val, cell_t, cell_v in kpis:
        ws_summary[cell_t] = title
        ws_summary[cell_t].font = Font(name="Calibri", size=10, color="595959")
        ws_summary[cell_v] = val
        ws_summary[cell_v].font = Font(name="Calibri", size=14, bold=True, color="1F4E79")

    ws_summary["A12"] = "Ranking das Melhores Combinações de Parâmetros"
    ws_summary["A12"].font = subtitle_font

    df["Is_Success_Num"] = (df["Status"] == "SUCCESS").astype(int)
    param_cols = ["Preprocess", "Seed", "Epsilon_Num", "Refactor_Period"]
    
    best_combos = df.groupby(param_cols, as_index=False).agg(
        Total_Runs=("Run_ID", "count"),
        Successes=("Is_Success_Num", "sum"),
        Optimal_Matches=("Is_Optimal", "sum"),
        Avg_Time=("Time_Seconds", "mean")
    )

    best_combos["Success_Rate"] = best_combos["Successes"] / best_combos["Total_Runs"]
    best_combos["Accuracy"] = best_combos["Optimal_Matches"] / best_combos["Successes"]
    best_combos = best_combos.sort_values(by=["Accuracy", "Success_Rate", "Avg_Time"], ascending=[False, False, True])

    # Colunas com num/total
    comb_headers = [
        "Preprocess", "Seed", "Epsilon", "Refactor Period", "Qtd Testes", 
        "Sucessos (num/total)", "Ótimos Netlib (num/total)", "Taxa Sucesso", "Precisão Custo", "Tempo Médio (s)"
    ]
    for col_idx, text in enumerate(comb_headers, 1):
        cell = ws_summary.cell(row=14, column=col_idx, value=text)
        cell.fill = header_fill
        cell.font = header_font
        cell.alignment = Alignment(horizontal="center")

    for row_idx, r in enumerate(best_combos.head(10).itertuples(), 15):
        eps_sci = fmt_sci(r.Epsilon_Num)
        suc_num_total = f"{r.Successes}/{r.Total_Runs}"
        opt_num_total = f"{r.Optimal_Matches}/{r.Total_Runs}"
        
        vals = [
            r.Preprocess, 
            r.Seed, 
            eps_sci, 
            r.Refactor_Period, 
            r.Total_Runs, 
            suc_num_total, 
            opt_num_total, 
            f"{r.Success_Rate:.1%}", 
            f"{r.Accuracy:.1%}", 
            round(r.Avg_Time, 4) if pd.notnull(r.Avg_Time) else "-"
        ]
        for col_idx, val in enumerate(vals, 1):
            cell = ws_summary.cell(row=row_idx, column=col_idx, value=val)
            cell.font = regular_font
            cell.border = thin_border
            if col_idx in [1, 2, 3, 4, 5, 6, 7]:
                cell.alignment = Alignment(horizontal="center")

    # ==================== ABA 2: VALIDAÇÃO NETLIB POR INSTÂNCIA ====================
    ws_best = wb.create_sheet(title="Validação Netlib por Instância")
    ws_best.views.sheetView[0].showGridLines = True

    ws_best["A1"] = "Comparação Custo Simplex vs. Valor Ótimo de Referência Netlib"
    ws_best["A1"].font = title_font

    best_per_inst_idx = df_success.groupby("Instance")["Time_Seconds"].idxmin()
    best_per_inst = df.loc[best_per_inst_idx].sort_values("Instance")

    # Parâmetros individualizados em colunas dedicadas
    best_headers = [
        "Instância", "Status Solver", "Custo Simplex", "Valor Ótimo Netlib", 
        "Erro Relativo", "Validação", "Tempo (s)", "Preprocess", "Seed", "Epsilon", "Refactor Period"
    ]
    for col_idx, h in enumerate(best_headers, 1):
        cell = ws_best.cell(row=3, column=col_idx, value=h)
        cell.fill = header_fill
        cell.font = header_font
        cell.alignment = Alignment(horizontal="center")

    for row_idx, (_, data) in enumerate(best_per_inst.iterrows(), 4):
        rel_diff = data["Rel_Diff"]
        
        if pd.isna(data["Optimal_Ref"]):
            val_status = "SEM REF NETLIB"
            fill_color = warning_fill
        elif data["Is_Optimal"]:
            val_status = "ÓTIMO (MATCH)"
            fill_color = success_fill
        else:
            val_status = "DIVERGENTE"
            fill_color = error_fill

        row_vals = [
            data["Instance"],
            data["Status"],
            round(data["Cost_Num"], 6) if pd.notnull(data["Cost_Num"]) else "-",
            round(data["Optimal_Ref"], 6) if pd.notnull(data["Optimal_Ref"]) else "N/A",
            f"{rel_diff:.2e}" if pd.notnull(rel_diff) else "N/A",
            val_status,
            round(data["Time_Seconds"], 6),
            data["Preprocess"],
            data["Seed"],
            fmt_sci(data["Epsilon_Num"]),
            data["Refactor_Period"]
        ]

        for col_idx, val in enumerate(row_vals, 1):
            cell = ws_best.cell(row=row_idx, column=col_idx, value=val)
            cell.font = regular_font
            cell.border = thin_border
            cell.fill = fill_color
            if col_idx in [8, 9, 10, 11]:
                cell.alignment = Alignment(horizontal="center")

    # ==================== ABA 3: DADOS COMPLETOS ====================
    ws_data = wb.create_sheet(title="Resultados Grid Search")
    ws_data.views.sheetView[0].showGridLines = True

    export_cols = ["Run_ID", "Instance", "Preprocess", "Seed", "Epsilon_Num", "Refactor_Period", "Time_Seconds", "Cost_Num", "Optimal_Ref", "Rel_Diff", "Status", "Is_Optimal"]
    ws_data.append(["Run ID", "Instância", "Preprocess", "Seed", "Epsilon", "Refactor Period", "Tempo (s)", "Custo Solver", "Ótimo Netlib", "Erro Relativo", "Status Solver", "Ótimo Confirmado"])

    for col_idx in range(1, len(export_cols) + 1):
        cell = ws_data.cell(row=1, column=col_idx)
        cell.fill = header_fill
        cell.font = header_font
        cell.alignment = Alignment(horizontal="center", vertical="center")

    for row_idx, row_data in enumerate(df[export_cols].values, 2):
        formatted_row = list(row_data)
        formatted_row[4] = fmt_sci(formatted_row[4])
        if pd.notnull(formatted_row[9]):
            formatted_row[9] = f"{formatted_row[9]:.2e}"
            
        ws_data.append(formatted_row)
        
        status = row_data[10]
        is_opt = row_data[11]

        for col_idx in range(1, len(export_cols) + 1):
            cell = ws_data.cell(row=row_idx, column=col_idx)
            cell.font = regular_font
            cell.border = thin_border
            if status == "SUCCESS" and is_opt:
                cell.fill = success_fill
            elif status == "SUCCESS" and not is_opt:
                cell.fill = warning_fill
            else:
                cell.fill = error_fill

    # Ajuste automático das colunas
    for sheet in wb.worksheets:
        for col in sheet.columns:
            max_len = max(len(str(cell.value or '')) for cell in col)
            col_letter = get_column_letter(col[0].column)
            sheet.column_dimensions[col_letter].width = max(max_len + 3, 12)

    wb.save(excel_file)
    print(f"📊 Relatório Excel gerado com sucesso: '{excel_file}'")

if __name__ == "__main__":
    generate_excel_report()
