#ifndef _QUAGGA_BGP_SVC_CONSTRAINT_H
#define _QUAGGA_BGP_SVC_CONSTRAINT_H

#include "bgpd/bgp_lcommunity.h"

#define BGP_SVC_CONSTRAINT_PREFIX ((uint8_t)85)

enum comparison_algorithm {
	BGP_SVC_CONSTRAINT_COMPUTE_TOTAL,
	BGP_SVC_CONSTRAINT_COMPUTE_COMMON,
	BGP_SVC_CONSTRAINT_CONFIGURED_COUNT,
};

enum constraint_type {
	BGP_SVC_CONSTRAINT_BASE = (BGP_SVC_CONSTRAINT_PREFIX << 24),
	BGP_SVC_CONSTRAINT_BANDWIDTH,
	BGP_SVC_CONSTRAINT_LATENCY,
	BGP_SVC_CONSTRAINT_NONE,
};

extern enum constraint_type bgp_parse_service_constraint_type(const char *str);
extern enum comparison_algorithm bgp_parse_service_constraint_comparison_algorithm(const char *str);
extern const char *bgp_service_comparison_comparison_algorithm(enum comparison_algorithm);

struct service_constraint_settings {
	char computation_algorithm;
	uint32_t weights[BGP_SVC_CONSTRAINT_NONE - BGP_SVC_CONSTRAINT_BASE - 1];
};

struct service_constraints {
	uint32_t constraints[BGP_SVC_CONSTRAINT_NONE - BGP_SVC_CONSTRAINT_BASE - 1];
};

extern struct service_constraint_settings *bgp_service_constraint_settings_init(void);

extern void bgp_configure_service_comparison_algorithm(struct bgp *, enum comparison_algorithm);
extern void bgp_configure_service_constraint_weights(struct bgp *, enum constraint_type type, uint32_t weight);

extern void bgp_configure_service_constraints(struct bgp *, enum constraint_type type, uint32_t value);
extern void peer_configure_service_constraints(struct peer *, enum constraint_type type, uint32_t value);

extern void bgp_apply_service_constraints(struct lcommunity **lcom, struct service_constraints *svc_constraints);

extern int bgp_update_service_constraints(struct lcommunity **lcom, struct service_constraints *svc_constraints);

extern int bgp_compare_service_constraints(struct service_constraint_settings *settings, struct lcommunity **lcom1,
					   struct lcommunity **lcom2);

#endif /* _QUAGGA_BGP_SVC_CONSTRAINT_H */
