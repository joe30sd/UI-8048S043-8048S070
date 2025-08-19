#ifndef EVENT_HANDLERS_H
#define EVENT_HANDLERS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief UI Event handlers for all screen interactions
 */

// Main navigation events
void map_event(lv_event_t *e);
void rdpb_event(lv_event_t *e);
void rupb_event(lv_event_t *e);

// Manual mode events
void mscd_event(lv_event_t *e);
void msvd_event(lv_event_t *e);
void mpsi_event(lv_event_t *e);
void mpsob_event(lv_event_t *e);
void mbb_event(lv_event_t *e);

// Relay control events
void axrb_event(lv_event_t *e);
void t1rb_event(lv_event_t *e);
void t2rb_event(lv_event_t *e);
void rdarb_event(lv_event_t *e);
void mainrb_event(lv_event_t *e);
void attob_event(lv_event_t *e);

// Operation control events
void usb_event(lv_event_t *e);
void ucb_event(lv_event_t *e);
void ossb_event(lv_event_t *e);
void oob_event(lv_event_t *e);

// Target control events
void tscd_event(lv_event_t *e);
void tsvd_event(lv_event_t *e);
void tpsi_event(lv_event_t *e);
void tpsob_event(lv_event_t *e);
void tob_event(lv_event_t *e);
void t_current_d(lv_event_t *e);
void t_current_i(lv_event_t *e);

// Down ramp events
void dsb_event(lv_event_t *e);
void dcb_event(lv_event_t *e);

// Utility events
void mbp_event(lv_event_t *e);
void umanic_event(lv_event_t *e);
void ut1c_event(lv_event_t *e);
void ut2c_event(lv_event_t *e);
void uaxc_event(lv_event_t *e);
void urdac_event(lv_event_t *e);
void rurcb_event(lv_event_t *e);

// Confirmation events
void confirm_yes(lv_event_t *e);
void confirm_no(lv_event_t *e);

// Reset and probe events
void ResetOperation(lv_event_t *e);
void rub_event(lv_event_t *e);
void probe_event(lv_event_t *e);
void rda_enb_event(lv_event_t *e);

// Diagnostic events
void sccb_event(lv_event_t *e);
void mccb_event(lv_event_t *e);
void rccb_event(lv_event_t *e);
void pccb_event(lv_event_t *e);
void pqcb_event(lv_event_t *e);
void pfcb_event(lv_event_t *e);
void dpb_event(lv_event_t *e);
void dbb_event(lv_event_t *e);

// Update events
void dbub_event(lv_event_t *e);
void mbub_event(lv_event_t *e);

#ifdef __cplusplus
}
#endif

#endif // EVENT_HANDLERS_H