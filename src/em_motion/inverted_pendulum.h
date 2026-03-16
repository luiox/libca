
// 职责：根据系统模型计算所需的力/力矩，用于平衡、补偿或前馈控制。
// 倒立摆：根据当前角度、角速度、目标角度，计算所需力矩
float inverted_pendulum_control(float angle, float ang_vel, float target_angle, float Kp, float Kd, float inertia);

// 或者更高级：包含重力矩补偿
float inverted_pendulum_torque(float angle, float mass, float length, float gravity);
