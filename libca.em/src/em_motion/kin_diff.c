
/// @file kin_diff.c
/// @author canrad (1517807724@qq.com)
/// @brief 差速底盘运动学基础实现
/// @version 0.1
/// @date 2026-03-16
///
/// @copyright Copyright (c) 2026
///

#include "kin_diff.h"

#include <math.h>

void diff_drive_ik(f32 v, f32 w, f32 wheel_base, f32* left, f32* right)
{
	if (!left || !right || wheel_base <= 0.0f) {
		return;
	}

	*right = v + (w * wheel_base * 0.5f);
	*left = v - (w * wheel_base * 0.5f);
}

void diff_drive_fk(f32 left, f32 right, f32 wheel_base, f32* v, f32* w)
{
	if (!v || !w || wheel_base <= 0.0f) {
		return;
	}

	*v = 0.5f * (right + left);
	*w = (right - left) / wheel_base;
}

void diff_drive_odometry(f32 left, f32 right, f32 wheel_base, f32 dt, pose2d_t* pose)
{
	f32 v = 0.0f;
	f32 w = 0.0f;

	if (!pose || dt <= 0.0f || wheel_base <= 0.0f) {
		return;
	}

	diff_drive_fk(left, right, wheel_base, &v, &w);

	pose->x += v * dt * cosf(pose->theta);
	pose->y += v * dt * sinf(pose->theta);
	pose->theta += w * dt;
}
