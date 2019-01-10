#include "pheeno_ros/pheeno_robot.h"

/*
 * Contructor for the PheenoRobot Class.
 *
 * The class constructor fills all std::vector variables with zeros
 * for use by the setters. Publishers and Subscribers are formally
 * defined as well.
 *
 */
PheenoRobot::PheenoRobot(std::string pheeno_name)
{
  ROS_INFO("Creating Pheeno Robot.");
  pheeno_namespace_id_ = pheeno_name;

  // Create IR sensor vector upon construction. (6 placements)
  for (int i = 0; i < 6; i++)
  {
    ir_sensor_vals_.push_back(0);
  }

  // Create sensor vector upon construction. (3 placements)
  for (int i = 0; i < 3; i++)
  {
    odom_pose_position_.push_back(0);
    odom_twist_linear_.push_back(0);
    odom_twist_angular_.push_back(0);

    magnetometer_vals_.push_back(0);
    gyroscope_vals_.push_back(0);
    accelerometer_vals_.push_back(0);
  }

  // Create sensor vector upon construction. (4 placements)
  for (int i = 0; i < 4; i++)
  {
    odom_pose_orient_.push_back(0);
    encoder_vals_.push_back(0);
  }

  // Use rosparams to fill defaults.
  if (nh_.hasParam("/pheeno_robot/default_linear_velocity"))
  {
    nh_.getParam("/pheeno_robot/default_linear_velocity", def_linear_vel_);
  }
  else
  {
    def_linear_vel_ = 0.5;
  }
  if (nh_.hasParam("/pheeno_robot/default_angular_velocity"))
  {
    nh_.getParam("/pheeno_robot/default_angular_velocity", def_angular_vel_);
  }
  else
  {
    def_angular_vel_ = 0.5;
  }
  if (nh_.hasParam("/pheeno_robot/obstacle_linear_velocity"))
  {
    nh_.getParam("/pheeno_robot/obstacle_linear_velocity", obs_linear_vel_);
  }
  else
  {
    obs_linear_vel_ = 0.5;
  }
  if (nh_.hasParam("/pheeno_robot/obstacle_angular_velocity"))
  {
    nh_.getParam("/pheeno_robot/obstacle_angular_velocity", obs_angular_vel_);
  }
  else
  {
    obs_angular_vel_ = 0.5;
  }

  // Set the angular and linear velocity variables the default values.
  linear_vel_ = def_linear_vel_;
  angular_vel_ = def_angular_vel_;

  // IR Sensor Subscribers
  sub_ir_center_ = nh_.subscribe<std_msgs::Float32>(pheeno_name + "/scan_center", 10,
    boost::bind(&PheenoRobot::irSensorCallback, this, _1, Pheeno::IR::CENTER));
  sub_ir_right_ = nh_.subscribe<std_msgs::Float32>(pheeno_name + "/scan_right", 10,
    boost::bind(&PheenoRobot::irSensorCallback, this, _1, Pheeno::IR::RIGHT));
  sub_ir_left_ = nh_.subscribe<std_msgs::Float32>(pheeno_name + "/scan_left", 10,
    boost::bind(&PheenoRobot::irSensorCallback, this, _1, Pheeno::IR::LEFT));
  sub_ir_cr_ = nh_.subscribe<std_msgs::Float32>(pheeno_name + "/scan_cr", 10,
    boost::bind(&PheenoRobot::irSensorCallback, this, _1, Pheeno::IR::CRIGHT));
  sub_ir_cl_ = nh_.subscribe<std_msgs::Float32>(pheeno_name + "/scan_cl", 10,
    boost::bind(&PheenoRobot::irSensorCallback, this, _1, Pheeno::IR::CLEFT));
  sub_ir_back_ = nh_.subscribe<std_msgs::Float32>(pheeno_name + "/scan_back", 10,
    boost::bind(&PheenoRobot::irSensorCallback, this, _1, Pheeno::IR::BACK));

  // Odom Subscriber
  sub_odom_ = nh_.subscribe(pheeno_name + "/odom", 1, &PheenoRobot::odomCallback, this);

  // Encoder Subscribers
  sub_encoder_LL_ = nh_.subscribe<std_msgs::Int16>(pheeno_name + "/encoder_LL", 10,
    boost::bind(&PheenoRobot::encoderCallback, this, _1, Pheeno::ENCODER::LL));
  sub_encoder_LR_ = nh_.subscribe<std_msgs::Int16>(pheeno_name + "/encoder_LR", 10,
    boost::bind(&PheenoRobot::encoderCallback, this, _1, Pheeno::ENCODER::LR));
  sub_encoder_RL_ = nh_.subscribe<std_msgs::Int16>(pheeno_name + "/encoder_RL", 10,
    boost::bind(&PheenoRobot::encoderCallback, this, _1, Pheeno::ENCODER::RL));
  sub_encoder_RR_ = nh_.subscribe<std_msgs::Int16>(pheeno_name + "/encoder_RR", 10,
    boost::bind(&PheenoRobot::encoderCallback, this, _1, Pheeno::ENCODER::RR));

  // Magnetometer, Gyroscope, Accelerometer Subscriber
  sub_magnetometer_ = nh_.subscribe(pheeno_name + "/magnetometer", 10, &PheenoRobot::magnetometerCallback, this);
  sub_gyroscope_ = nh_.subscribe(pheeno_name + "/gyroscope", 10, &PheenoRobot::gyroscopeCallback, this);
  sub_accelerometer_ = nh_.subscribe(pheeno_name + "/accelerometer", 10, &PheenoRobot::accelerometerCallback, this);

  // cmd_vel Publisher
  pub_cmd_vel_ = nh_.advertise<geometry_msgs::Twist>(pheeno_name + "/cmd_vel", 100);

}

/*
 * Publishes command velocity (cmd_vel) messages.
 *
 * The specific Topic name that the message is published to is defined
 * in the Constructor for the PheenoRobot class.
 */
void PheenoRobot::publishCmdVelocity(geometry_msgs::Twist velocity)
{
  pub_cmd_vel_.publish(velocity);
}

/*
 * Callback function for the IR Sensors' ROS subscriber.
 */
void PheenoRobot::irSensorCallback(const std_msgs::Float32::ConstPtr& msg, int ir_location)
{
  ir_sensor_vals_[ir_location] = static_cast<double>(msg->data);
}

/*
 * Given a specific sensor limit, this member function returns a bool if
 * any of the sensors within ir_sensor_values is triggered.
 */
bool PheenoRobot::irSensorTriggered(float sensor_limit)
{
  int count = 0;
  for (int i = 0; i < 7; i++)
  {
    if (ir_sensor_vals_[i] < sensor_limit)
    {
      count += 1;
    }
  }

  // Greater than 1 instead of 0, because we aren't using the back IR sensor.
  return (count > 1) ? true : false;
}

/*
 * Callback function for the odom ROS subscriber.
 *
 * This will set message data to odom pose (position and orientation) and twist
 * (linear and angular) variables.
 *
 * NOTE: ONLY USED IF `libgazebo_ros_p3d.so` PLUGIN IS IN THE XACRO FILE.
 */
void PheenoRobot::odomCallback(const nav_msgs::Odometry::ConstPtr& msg)
{
  // Assign values to appropriate pose information.
  odom_pose_position_[0] = static_cast<double>(msg->pose.pose.position.x);
  odom_pose_position_[1] = static_cast<double>(msg->pose.pose.position.y);
  odom_pose_position_[2] = static_cast<double>(msg->pose.pose.position.z);
  odom_pose_orient_[0] = static_cast<double>(msg->pose.pose.orientation.x);
  odom_pose_orient_[1] = static_cast<double>(msg->pose.pose.orientation.y);
  odom_pose_orient_[2] = static_cast<double>(msg->pose.pose.orientation.z);
  odom_pose_orient_[3] = static_cast<double>(msg->pose.pose.orientation.w);

  // Assign values to appropriate twist information.
  odom_twist_linear_[0] = static_cast<double>(msg->twist.twist.linear.x);
  odom_twist_linear_[1] = static_cast<double>(msg->twist.twist.linear.y);
  odom_twist_linear_[2] = static_cast<double>(msg->twist.twist.linear.z);
  odom_twist_angular_[0] = static_cast<double>(msg->twist.twist.angular.x);
  odom_twist_angular_[1] = static_cast<double>(msg->twist.twist.angular.y);
  odom_twist_angular_[2] = static_cast<double>(msg->twist.twist.angular.z);
}

/*
 * Callback function for the Encoders connected to each H-bridge ROS Subscriber.
 */
void PheenoRobot::encoderCallback(const std_msgs::Int16::ConstPtr& msg, int encoder_location)
{
  encoder_vals_[encoder_location] = static_cast<int>(msg->data);
}

/*
 * Callback function for the Magnetometer sensor ROS subscriber.
 */
void PheenoRobot::magnetometerCallback(const geometry_msgs::Vector3::ConstPtr& msg)
{
  magnetometer_vals_[0] = static_cast<double>(msg->x);
  magnetometer_vals_[1] = static_cast<double>(msg->y);
  magnetometer_vals_[2] = static_cast<double>(msg->z);
}

/*
 * Callback function for the Gyroscope sensor ROS subscriber.
 */
void PheenoRobot::gyroscopeCallback(const geometry_msgs::Vector3::ConstPtr& msg)
{
  gyroscope_vals_[0] = static_cast<double>(msg->x);
  gyroscope_vals_[1] = static_cast<double>(msg->y);
  gyroscope_vals_[2] = static_cast<double>(msg->z);
}

/*
 * Callback function for the Accelerometer sensor ROS subscriber.
 */
void PheenoRobot::accelerometerCallback(const geometry_msgs::Vector3::ConstPtr& msg)
{
  accelerometer_vals_[0] = static_cast<double>(msg->x);
  accelerometer_vals_[1] = static_cast<double>(msg->y);
  accelerometer_vals_[2] = static_cast<double>(msg->z);
}

/*
 * Callback function for the Pi Cam ROS subscriber.
 *
 * Will recieve sensor_msgs.Image() messages and save only the current frame. If
 * the user needs to do image manipulation, use the CV_BRIDGE ros package to
 * convert the sensor_msgs.Image() data type to one useable by OpenCV.
 */
void PheenoRobot::piCamCallback()
{
  ROS_INFO("Not in use yet.");
}

/*
 * Generates a random turn direction (a sign change) based on a user provided value.
 */
double PheenoRobot::randomTurn(double angular)
{
  return rand() % 10 + 1 <= 5 ? (-1.0 * angular) : angular;
}

/*
 *
 */
double PheenoRobot::getDefaultLinearVelocity()
{
  return def_linear_vel_;
}

/*
 *
 */
double PheenoRobot::getDefaultAngularVelocity()
{
  return def_angular_vel_;
}

/*
 *
 */
void PheenoRobot::setDefaultLinearVelocity(double new_linear_velocity)
{
  def_linear_vel_ = new_linear_velocity;
}

/*
 *
 */
void PheenoRobot::setDefaultAngularVelocity(double new_angular_velocity)
{
  def_angular_vel_ = new_angular_velocity;
}

/*
 *
 */
double PheenoRobot::getObstacleLinearVelocity()
{
  return obs_linear_vel_;
}

/*
 *
 */
double PheenoRobot::getObstacleAngularVelocity()
{
  return obs_angular_vel_;
}

/*
 *
 */
void PheenoRobot::setObstacleLinearVelocity(double new_linear_velocity)
{
  obs_linear_vel_ = new_linear_velocity;
}

/*
 *
 */
void PheenoRobot::setObstacleAngularVelocity(double new_angular_velocity)
{
  obs_angular_vel_ = new_angular_velocity;
}

/*
 * Obstacle avoidance logic for a robot moving in an angular (turning) motion.
 *
 * Using class specific IR values, this simple if-else logic progresses by
 * comparing them to a certain range to avoid (default value). If triggered,
 * the references to angular are set to specific values to make the robot avoid
 * the obstacle. Compared to the linear version of this callback member
 * function, this function only modifies the angular reference.
 */
bool PheenoRobot::avoidObstacleMove(double& linear, double& angular, double range_to_avoid)
{
  // double new_angular_vel;
  bool obs_flag = false;

  // if (angular == 0)
  // {
  //   new_angular_vel = obs_angular_vel_;
  // }
  // else
  // {
  //   new_angular_vel = angular;
  // }

  // If Center IR Tripped
  if (ir_sensor_vals_[Pheeno::IR::CENTER] < range_to_avoid)
  {
    if (std::abs((ir_sensor_vals_[Pheeno::IR::RIGHT] - ir_sensor_vals_[Pheeno::IR::LEFT])) < 5.0 ||
        (ir_sensor_vals_[Pheeno::IR::RIGHT] > range_to_avoid && ir_sensor_vals_[Pheeno::IR::LEFT] > range_to_avoid))
    {
      linear = obs_linear_vel_;
      angular = randomTurn(obs_angular_vel_);
    }

    if (ir_sensor_vals_[Pheeno::IR::RIGHT] < ir_sensor_vals_[Pheeno::IR::LEFT])
    {
      angular = -1.0 * obs_angular_vel_;  // Turn Left
    }
    else
    {
      angular = obs_angular_vel_;  // Turn Right
    }

    // Set obstacle flag
    obs_flag = true;
  }
  // If both CRight AND CLeft IR sensors tripped.
  else if (ir_sensor_vals_[Pheeno::IR::CRIGHT] < range_to_avoid && ir_sensor_vals_[Pheeno::IR::CLEFT] < range_to_avoid)
  {
    angular = randomTurn(obs_angular_vel_);
    obs_flag = true;
  }
  // If only CRight IR sensor tripped.
  else if (ir_sensor_vals_[Pheeno::IR::CRIGHT] < range_to_avoid)
  {
    angular = -1.0 * obs_angular_vel_;  // Turn Left
    obs_flag = true;
  }
  // If only CLeft IR sensor tripped.
  else if (ir_sensor_vals_[Pheeno::IR::CLEFT] < range_to_avoid)
  {
    angular = obs_angular_vel_;  // Turn Right
    obs_flag = true;
  }
  // If only Right IR sensor tripped.
  else if (ir_sensor_vals_[Pheeno::IR::RIGHT] < range_to_avoid)
  {
    angular = -1.0 * obs_angular_vel_;  // Turn Left
    obs_flag = true;
  }
  // If only Left IR sensor tripped.
  else if (ir_sensor_vals_[Pheeno::IR::LEFT] < range_to_avoid)
  {
    angular = obs_angular_vel_;  // Turn Right
    obs_flag = true;
  }

  if (obs_flag)
  {
    linear = obs_linear_vel_;  // Set linear to the obstacle avoidance value.
    ROS_INFO("Obstacle detected: avoidObstacleMove()");
  }

  return obs_flag;

}

/*
 * Obstacle avoidance logic for a robot moving in a linear motion.
 *
 * Using class specific IR values, this simple if-else logic progresses by
 * comparing them to a certain range to avoid (default value). If triggered,
 * the references to linear and angular are set to specific values to make the
 * robot avoid the obstacle.
 */
bool PheenoRobot::avoidObstacleStop(double& linear, double& angular, double range_to_avoid)
{
  // double new_angular_vel;
  bool obs_flag = false;

  // if (angular == 0)
  // {
  //   new_angular_vel = obs_angular_vel_;
  // }
  // else
  // {
  //   new_angular_vel = angular;
  // }

  // If Center IR Tripped
  if (ir_sensor_vals_[Pheeno::IR::CENTER] < range_to_avoid)
  {
    if (ir_sensor_vals_[Pheeno::IR::RIGHT] < ir_sensor_vals_[Pheeno::IR::LEFT])
    {
      angular = -1.0 * obs_angular_vel_; // Turn Left
    }
    else
    {
      angular = obs_angular_vel_; // Turn Right
    }

    // Set obstacle flag
    obs_flag = true;
  }
  // If both CRight AND CLeft IR sensors tripped.
  else if (ir_sensor_vals_[Pheeno::IR::CRIGHT] < range_to_avoid && ir_sensor_vals_[Pheeno::IR::CLEFT] < range_to_avoid)
  {
    angular = randomTurn(obs_angular_vel_);
    obs_flag = true;
  }
  // If only CRight IR sensor tripped.
  else if (ir_sensor_vals_[Pheeno::IR::CRIGHT] < range_to_avoid)
  {
    angular = -1.0 * obs_angular_vel_; // Turn Left
    obs_flag = true;
  }
  // If only CLeft IR sensor tripped.
  else if (ir_sensor_vals_[Pheeno::IR::CLEFT] < range_to_avoid)
  {
    angular = obs_angular_vel_; // Turn Right
    obs_flag = true;
  }
  // If only Right IR sensor tripped.
  else if (ir_sensor_vals_[Pheeno::IR::RIGHT] < range_to_avoid)
  {
    angular = -1.0 * obs_angular_vel_; // Turn Left
    obs_flag = true;
  }
  // If only Left IR sensor tripped.
  else if (ir_sensor_vals_[Pheeno::IR::LEFT] < range_to_avoid)
  {
    angular = obs_angular_vel_; // Turn Right
    obs_flag = true;
  }

  if (obs_flag)
  {
    linear = 0.0; // Set Linear to 0.
    ROS_INFO("Obstacle detected: avoidObstacleStop()");
  }

  return obs_flag;

}
