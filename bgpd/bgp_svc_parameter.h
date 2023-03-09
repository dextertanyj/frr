#ifndef _QUAGGA_BGP_SVC_PARAMETER_H
#define _QUAGGA_BGP_SVC_PARAMETER_H

#include "bgpd/bgp_lcommunity.h"

#define BGP_SVC_PARAMETER_PREFIX ((uint8_t)85)

enum comparison_algorithm {
	BGP_SVC_PARAMETER_COMPUTE_TOTAL,
	BGP_SVC_PARAMETER_COMPUTE_COMMON,
	BGP_SVC_PARAMETER_CONFIGURED_COUNT,
};

enum parameter_type {
	BGP_SVC_PARAMETER_BASE = (BGP_SVC_PARAMETER_PREFIX << 24),
	BGP_SVC_PARAMETER_BANDWIDTH,
	BGP_SVC_PARAMETER_LATENCY,
	BGP_SVC_PARAMETER_NONE,
};

extern enum parameter_type bgp_parse_service_parameter_type(const char *str);
extern enum comparison_algorithm bgp_parse_service_parameter_comparison_algorithm(const char *str);
extern const char *bgp_service_comparison_comparison_algorithm(enum comparison_algorithm);

struct service_parameter_settings {
	char computation_algorithm;
	uint32_t weights[BGP_SVC_PARAMETER_NONE - BGP_SVC_PARAMETER_BASE - 1];
};

struct service_parameters {
	uint32_t parameters[BGP_SVC_PARAMETER_NONE - BGP_SVC_PARAMETER_BASE - 1];
};

extern struct service_parameter_settings *bgp_service_parameter_settings_init(void);

extern void bgp_configure_service_comparison_algorithm(struct bgp *, enum comparison_algorithm);
extern void bgp_configure_service_parameter_weights(struct bgp *, enum parameter_type type, uint32_t weight);

extern void bgp_configure_service_parameters(struct bgp *, enum parameter_type type, uint32_t value);
extern void peer_configure_service_parameters(struct peer *, enum parameter_type type, uint32_t value);

extern void bgp_apply_service_parameters(struct lcommunity **lcom, struct service_parameters *svc_parameters);

extern int bgp_update_service_parameters(struct lcommunity **lcom, struct service_parameters *svc_parameters);

extern int bgp_compare_service_parameters(struct service_parameter_settings *settings, struct lcommunity **lcom1,
					  struct lcommunity **lcom2);

#endif /* _QUAGGA_BGP_SVC_PARAMETERS_H */
