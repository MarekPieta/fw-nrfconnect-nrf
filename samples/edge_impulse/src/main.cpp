#include <zephyr.h>

#include "features.h"
#include <ei_ncs.h>


void result_ready_cb(int err)
{
	int e = ei_ncs_add_sample_data(features, ARRAY_SIZE(features));
	__ASSERT_NO_MSG(!e);
	e = ei_ncs_move_input_window(ARRAY_SIZE(features)/ei_ncs_get_frame_size());
	__ASSERT_NO_MSG(!e);
	k_msleep(1000);
	e = ei_ncs_start_prediction(result_ready_cb);
	__ASSERT_NO_MSG(!e);
}

void main(void)
{
	ei_ncs_init();
	int e = ei_ncs_add_sample_data(features, ARRAY_SIZE(features));
	__ASSERT_NO_MSG(!e);
	e = ei_ncs_start_prediction(result_ready_cb);
	__ASSERT_NO_MSG(!e);
}
