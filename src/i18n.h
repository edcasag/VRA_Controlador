// i18n por build flag: -DLANG_PT (default) ou -DLANG_EN.
// Espelha _python/src/i18n.py mas sem overhead de runtime — todas as
// strings sao macros de literal C, resolvidas em compile-time.
#pragma once

#if defined(LANG_EN) && !defined(LANG_PT)

// =============== Menu Serial ===============
#define MSG_BANNER             "=== POC VRA_Controlador (env: sim) ==="
#define MSG_MOUNT_FAIL         "[ERROR] LittleFS failed to mount"
#define MSG_KMLS_FOUND         "KML files found in /:"
#define MSG_NO_KMLS            "[ERROR] No .kml files in LittleFS. Run 'pio run -t uploadfs' first."
#define MSG_CSV_AVAILABLE      "[CSV ground truth: YES]"
#define MSG_CSV_MISSING        "[CSV ground truth: no ]"
#define MSG_CHOOSE_PROMPT      "Choose [1-%d]: "
#define MSG_INVALID_CHOICE     "Invalid choice, try again."
#define MSG_SINGLE_KML         "(only KML available)"
#define MSG_LOADED_KML         "Loaded: %s (%d zones)"
#define MSG_LOADED_KML_FAIL    "[ERROR] failed to load %s"
#define MSG_LOADED_CSV         "Loaded: %s (%d fixes, with Python dose)"
#define MSG_NO_CSV             "No ground truth CSV — running standalone"
#define MSG_SIM_STARTING       "Simulating: "
#define MSG_SIM_DONE           " done"

// =============== Relatorio ===============
#define MSG_REPORT_TITLE       "=== FINAL REPORT ==="
#define MSG_COL_ZONE           "Zone"
#define MSG_COL_TARGET         "Target (kg/ha)"
#define MSG_COL_APPLIED        "Applied avg"
#define MSG_COL_ERROR_PCT      "Error %"
#define MSG_COL_AREA           "Area covered (m2)"
#define MSG_GLOBALS            "Globals:"
#define MSG_GLOBAL_ERROR       "  Weighted global error %%: %.2f%%"
#define MSG_LATENCY_DOSE       "  logica.dose() latency:   ~%.1f us / call"
#define MSG_LATENCY_PID        "  PID iteration latency:   ~%.1f us / iter"
#define MSG_PARITY_TITLE       "Parity Python <-> ESP32 (%d points):"
#define MSG_PARITY_MAXDIFF     "  max |delta_dose|: %.2e kg/ha"
#define MSG_PARITY_DIVERGE     "  divergences > 1e-3 kg/ha: %d"
#define MSG_PARITY_VERDICT     "  verdict: %s"
#define MSG_VERDICT_PASS       "PASS"
#define MSG_VERDICT_FAIL       "FAIL"
#define MSG_END                "=== END ==="

#else  // default LANG_PT

// =============== Menu Serial ===============
#define MSG_BANNER             "=== POC VRA_Controlador (env: sim) ==="
#define MSG_MOUNT_FAIL         "[ERRO] LittleFS nao montou"
#define MSG_KMLS_FOUND         "KMLs encontrados em /:"
#define MSG_NO_KMLS            "[ERRO] Nenhum .kml no LittleFS. Rode 'pio run -t uploadfs' primeiro."
#define MSG_CSV_AVAILABLE      "[CSV ground truth: SIM]"
#define MSG_CSV_MISSING        "[CSV ground truth: nao]"
#define MSG_CHOOSE_PROMPT      "Escolha [1-%d]: "
#define MSG_INVALID_CHOICE     "Escolha invalida, tente novamente."
#define MSG_SINGLE_KML         "(unico KML disponivel)"
#define MSG_LOADED_KML         "Carregado: %s (%d zonas)"
#define MSG_LOADED_KML_FAIL    "[ERRO] falha ao carregar %s"
#define MSG_LOADED_CSV         "Carregado: %s (%d fixes, com dose Python)"
#define MSG_NO_CSV             "Sem CSV ground truth — rodando standalone"
#define MSG_SIM_STARTING       "Simulando: "
#define MSG_SIM_DONE           " concluido"

// =============== Relatorio ===============
#define MSG_REPORT_TITLE       "=== RELATORIO FINAL ==="
#define MSG_COL_ZONE           "Zona"
#define MSG_COL_TARGET         "Alvo (kg/ha)"
#define MSG_COL_APPLIED        "Aplicado medio"
#define MSG_COL_ERROR_PCT      "Erro %"
#define MSG_COL_AREA           "Area coberta (m2)"
#define MSG_GLOBALS            "Globais:"
#define MSG_GLOBAL_ERROR       "  Erro %% global ponderado: %.2f%%"
#define MSG_LATENCY_DOSE       "  Latencia logica.dose():  ~%.1f us / chamada"
#define MSG_LATENCY_PID        "  Latencia PID:            ~%.1f us / iteracao"
#define MSG_PARITY_TITLE       "Paridade Python <-> ESP32 (%d pontos):"
#define MSG_PARITY_MAXDIFF     "  max |delta_dose|: %.2e kg/ha"
#define MSG_PARITY_DIVERGE     "  divergencias > 1e-3 kg/ha: %d"
#define MSG_PARITY_VERDICT     "  veredicto: %s"
#define MSG_VERDICT_PASS       "PASS"
#define MSG_VERDICT_FAIL       "FAIL"
#define MSG_END                "=== FIM ==="

#endif
