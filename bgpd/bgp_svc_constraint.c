#include <zebra.h>

#include "memory.h"

#include "bgpd/bgpd.h"
#include "bgpd/bgp_svc_constraint.h"

static uint32_t update_bandwidth(uint32_t existing, uint32_t constraint)
{
	return existing > constraint ? constraint : existing;
}

static uint32_t update_latency(uint32_t existing, uint32_t constraint)
{
	return existing + constraint;
}

uint32_t (*UPDATE_FUNCTIONS[])(uint32_t, uint32_t) = {
	update_bandwidth,
	update_latency,
};

int COMPARISON_MULTIPLER[] = {
	1,  // BGP_SVC_CONSTRAINT_BANDWIDTH
	-1, // BGP_SVC_CONSTRAINT_LATENCY
};

enum constraint_type bgp_parse_service_constraint_type(const char *str)
{
	if (!str) {
		return BGP_SVC_CONSTRAINT_NONE;
	}
	if (strmatch(str, "bandwidth")) {
		return BGP_SVC_CONSTRAINT_BANDWIDTH;
	}
	if (strmatch(str, "latency")) {
		return BGP_SVC_CONSTRAINT_LATENCY;
	}
	return BGP_SVC_CONSTRAINT_BASE;
}

enum comparison_algorithm bgp_parse_service_constraint_comparison_algorithm(const char *str)
{
	if (!str) {
		return -1;
	}
	if (strmatch(str, "all")) {
		return BGP_SVC_CONSTRAINT_COMPUTE_TOTAL;
	}
	if (strmatch(str, "common")) {
		return BGP_SVC_CONSTRAINT_COMPUTE_COMMON;
	}
	if (strmatch(str, "count")) {
		return BGP_SVC_CONSTRAINT_CONFIGURED_COUNT;
	}
	return -1;
}

const char *bgp_service_comparison_comparison_algorithm(enum comparison_algorithm algorithm)
{
	switch (algorithm) {
	case BGP_SVC_CONSTRAINT_COMPUTE_TOTAL:
		return "all";
	case BGP_SVC_CONSTRAINT_COMPUTE_COMMON:
		return "common";
	case BGP_SVC_CONSTRAINT_CONFIGURED_COUNT:
		return "count";
	default:
		return "unknown algorithm";
	}
}

struct service_constraint_settings *bgp_service_constraint_settings_init(void)
{
	struct service_constraint_settings *ptr =
		XCALLOC(MTYPE_SERVICE_CONSTRAINTS, sizeof(struct service_constraint_settings));
	for (size_t idx = 0; idx < 2; idx++) {
		ptr->weights[idx] = 1;
	}
	return ptr;
}

void bgp_configure_service_comparison_algorithm(struct bgp *bgp, enum comparison_algorithm algorithm)
{
	if (!bgp->service_constraint_settings) {
		bgp->service_constraint_settings = bgp_service_constraint_settings_init();
	}
	bgp->service_constraint_settings->computation_algorithm = algorithm;
}

void bgp_configure_service_constraint_weights(struct bgp *bgp, enum constraint_type type, uint32_t weight)
{
	if (!bgp->service_constraint_settings) {
		bgp->service_constraint_settings = bgp_service_constraint_settings_init();
	}
	bgp->service_constraint_settings->weights[type - BGP_SVC_CONSTRAINT_BASE - 1] = weight;
}

static void configure_service_constraint(struct service_constraints *svc_constraints, enum constraint_type type,
					 uint32_t value)
{
	svc_constraints->constraints[type - BGP_SVC_CONSTRAINT_BASE - 1] = value;
}

void bgp_configure_service_constraints(struct bgp *bgp, enum constraint_type type, uint32_t value)
{
	if (!bgp->service_constraints) {
		bgp->service_constraints = XCALLOC(MTYPE_SERVICE_CONSTRAINTS, sizeof(struct service_constraints));
	}
	configure_service_constraint(bgp->service_constraints, type, value);
}

void peer_configure_service_constraints(struct peer *peer, enum constraint_type type, uint32_t value)
{
	if (!peer->service_constraints) {
		peer->service_constraints = XCALLOC(MTYPE_SERVICE_CONSTRAINTS, sizeof(struct service_constraints));
	}
	configure_service_constraint(peer->service_constraints, type, value);
}


static void create_svc_constraint_prefix(enum constraint_type type, uint8_t **result, size_t *result_size)
{
	if (type == BGP_SVC_CONSTRAINT_NONE) {
		*result = XCALLOC(MTYPE_SERVICE_CONSTRAINTS, 5 * sizeof(uint8_t));
		(*result)[4] = BGP_SVC_CONSTRAINT_PREFIX;
		*result_size = 5;
		return;
	}
	*result = XCALLOC(MTYPE_SERVICE_CONSTRAINTS, 8 * sizeof(uint8_t));
	uint8_t *prefix = (uint8_t *)(&type);
	for (size_t idx = 0; idx < 4; idx++) {
		(*result)[4 + idx] = *(prefix + 3 - idx);
	}
	*result_size = 8;
	return;
}

static uint32_t extract_svc_constraint_val(struct lcommunity_val *lcom_val)
{
	uint32_t value = 0;
	uint8_t *ptr = (uint8_t *)&value;
	for (size_t idx = 0; idx < (sizeof(uint32_t) / sizeof(uint8_t)); idx++) {
		ptr[idx] = lcom_val->val[sizeof(lcom_val->val) - 1 - idx];
	}
	return value;
}

static void create_svc_constraint_val(uint32_t value, uint8_t **result)
{
	*result = XCALLOC(MTYPE_SERVICE_CONSTRAINTS, sizeof(uint32_t));
	uint8_t *val = (uint8_t *)(&value);
	for (size_t idx = 0; idx < (sizeof(uint32_t) / sizeof(uint8_t)); idx++) {
		(*result)[idx] = val[sizeof(uint32_t) - 1 - idx];
	}
	return;
}

static struct lcommunity_val *create_svc_constraint_lcommunity_val(enum constraint_type type, uint32_t value)
{
	struct lcommunity_val *result = lcommunity_val_new();
	uint8_t *prefix = NULL;
	size_t prefix_size = 0;
	create_svc_constraint_prefix(type, &prefix, &prefix_size);
	memcpy(result->val, prefix, prefix_size);
	uint8_t *val = NULL;
	create_svc_constraint_val(value, &val);
	memcpy(result->val + prefix_size, val, sizeof(uint32_t) / sizeof(uint8_t));
	XFREE(MTYPE_SERVICE_CONSTRAINTS, prefix);
	XFREE(MTYPE_SERVICE_CONSTRAINTS, val);
	return result;
}

static struct lcommunity_val *search_svc_constraint_lcommunity_val(struct lcommunity *lcom, enum constraint_type type)
{
	uint8_t *prefix = NULL;
	size_t prefix_size = 0;
	create_svc_constraint_prefix(type, &prefix, &prefix_size);
	struct lcommunity_val *existing = lcommunity_search_val(lcom, prefix, prefix_size);
	XFREE(MTYPE_SERVICE_CONSTRAINTS, prefix);
	return existing;
}

void bgp_apply_service_constraints(struct lcommunity **lcom, struct service_constraints *svc_constraints)
{
	*lcom = lcommunity_new();
	for (uint32_t type = BGP_SVC_CONSTRAINT_BASE + 1; type < BGP_SVC_CONSTRAINT_NONE; type++) {
		if (!svc_constraints->constraints[type - BGP_SVC_CONSTRAINT_BASE - 1]) {
			continue;
		}
		struct lcommunity_val *val = create_svc_constraint_lcommunity_val(
			type, svc_constraints->constraints[type - BGP_SVC_CONSTRAINT_BASE - 1]);
		lcommunity_add_val(*lcom, val);
		lcommunity_val_free(val);
	}
}

int bgp_update_service_constraints(struct lcommunity **lcom, struct service_constraints *svc_constraints)
{
	if (!(svc_constraints)) {
		if (!(*lcom)) {
			return 0;
		}

		/* Alternatively, we can reject the route entirely if there are
		 * service constraints present. */
		*lcom = lcommunity_dup(*lcom);
		uint8_t *prefix = NULL;
		size_t prefix_size = 0;
		create_svc_constraint_prefix(BGP_SVC_CONSTRAINT_NONE, &prefix, &prefix_size);
		struct lcommunity_val *lcom_val = NULL;
		do {
			// Safe free since XFREE checks for nullptr.
			lcommunity_val_free(lcom_val);
			lcom_val = lcommunity_search_val(*lcom, prefix, prefix_size);
			if (lcom_val) {
				lcommunity_del_val(*lcom, (uint8_t *)lcom_val->val);
			}
		} while (lcom_val);
		lcommunity_val_free(lcom_val);
		if ((*lcom)->size == 0) {
			lcommunity_free(lcom);
			*lcom = 0;
			return 1;
		}
		return 0;
	}

	*lcom = lcommunity_dup(*lcom);
	for (uint64_t type = BGP_SVC_CONSTRAINT_BASE + 1; type < BGP_SVC_CONSTRAINT_NONE; type++) {
		struct lcommunity_val *existing = search_svc_constraint_lcommunity_val(*lcom, type);
		if (!existing) {
			continue;
		}
		uint32_t existing_val = extract_svc_constraint_val(existing);
		lcommunity_del_val(*lcom, (uint8_t *)existing->val);
		lcommunity_val_free(existing);
		if (!svc_constraints->constraints[type - BGP_SVC_CONSTRAINT_BASE - 1]) {
			continue;
		}
		uint32_t new_val = UPDATE_FUNCTIONS[type - BGP_SVC_CONSTRAINT_BASE - 1](
			existing_val, svc_constraints->constraints[type - BGP_SVC_CONSTRAINT_BASE - 1]);
		struct lcommunity_val *new = create_svc_constraint_lcommunity_val(type, new_val);
		lcommunity_add_val(*lcom, new);
		lcommunity_val_free(new);
	}
	return 0;
}

static int compare_algorithm_all(struct service_constraint_settings *settings, struct lcommunity *lcom1,
				 struct lcommunity *lcom2)
{
	int64_t result = 0; // Possibly negative.

	for (uint64_t type = BGP_SVC_CONSTRAINT_BASE + 1; type < BGP_SVC_CONSTRAINT_NONE; type++) {
		int64_t local_result = 0;
		struct lcommunity_val *existing = NULL;
		size_t type_idx = type - BGP_SVC_CONSTRAINT_BASE - 1;
		existing = search_svc_constraint_lcommunity_val(lcom1, type);
		if (existing) {
			local_result = (int64_t)extract_svc_constraint_val(existing);
			lcommunity_val_free(existing);
		}
		existing = search_svc_constraint_lcommunity_val(lcom2, type);
		if (existing) {
			local_result -= (int64_t)extract_svc_constraint_val(existing);
			lcommunity_val_free(existing);
		}
		result += local_result * (int64_t)settings->weights[type_idx] * COMPARISON_MULTIPLER[type_idx];
	}
	return result == 0 ? 0 : result > 0 ? 1 : -1;
}

static uint32_t compare_algorithm_common(struct service_constraint_settings *settings, struct lcommunity *lcom1,
					 struct lcommunity *lcom2)
{
	int64_t result = 0; // Possibly negative.

	for (uint64_t type = BGP_SVC_CONSTRAINT_BASE + 1; type < BGP_SVC_CONSTRAINT_NONE; type++) {
		struct lcommunity_val *existing1 = NULL, *existing2 = NULL;
		size_t type_idx = type - BGP_SVC_CONSTRAINT_BASE - 1;
		existing1 = search_svc_constraint_lcommunity_val(lcom1, type);
		existing2 = search_svc_constraint_lcommunity_val(lcom2, type);
		if (!existing1 || !existing2) {
			continue;
		}
		int64_t local_result =
			(int64_t)extract_svc_constraint_val(existing1) - (int64_t)extract_svc_constraint_val(existing2);
		lcommunity_val_free(existing1);
		lcommunity_val_free(existing2);
		result += local_result * (int64_t)settings->weights[type_idx] * COMPARISON_MULTIPLER[type_idx];
	}
	return result == 0 ? 0 : result > 0 ? 1 : -1;
}

static int compare_algorithm_count(struct service_constraint_settings *, struct lcommunity *lcom1,
				   struct lcommunity *lcom2)
{
	size_t count1 = 0, count2 = 0;
	for (uint64_t type = BGP_SVC_CONSTRAINT_BASE + 1; type < BGP_SVC_CONSTRAINT_NONE; type++) {
		struct lcommunity_val *existing = NULL;
		existing = search_svc_constraint_lcommunity_val(lcom1, type);
		if (existing) {
			count1++;
			lcommunity_val_free(existing);
		}
		existing = search_svc_constraint_lcommunity_val(lcom2, type);
		if (existing) {
			count2++;
			lcommunity_val_free(existing);
		}
	}
	return count1 == count2 ? 0 : count1 < count2 ? 1 : -1;
}

int (*COMPARE_FUNCTIONS[])(struct service_constraint_settings *, struct lcommunity *, struct lcommunity *) = {
	compare_algorithm_all,
	compare_algorithm_common,
	compare_algorithm_count,
};

extern int bgp_compare_service_constraints(struct service_constraint_settings *settings, struct lcommunity **lcom1,
					   struct lcommunity **lcom2)
{
	if (!*lcom1 && !lcom2) {
		return 0;
	}
	if (!*lcom1) {
		return -1;
	}
	if (!*lcom2) {
		return 1;
	}
	return COMPARE_FUNCTIONS[settings->computation_algorithm](settings, *lcom1, *lcom2);
}
