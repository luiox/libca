
// 职责：将机器人的期望速度（线速度、角速度）转换为各执行器的控制量（轮速、舵机角度），
// 或者将传感器数据（轮速）转换为里程计。

// 差速运动学：输入 v, w，输出左右轮速度
void diff_drive_ik(float v, float w, float wheel_base, float *left, float *right);

// 差速里程计：输入左右轮速度，输出 v, w 及位姿更新
void diff_drive_odometry(float left, float right, float wheel_base, float dt, pose2d_t *pose);
