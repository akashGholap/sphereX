/** @file demo_flight_control.h
 *  @version 3.3
 *  @date May, 2017
 *
 *  @brief
 *  demo sample of how to use flight control APIs
 *
 *  @copyright 2017 DJI. All rights reserved.
 *
 */

#ifndef SPHEREX_FLIGHT_CONTROL_H
#define SPHEREX_FLIGHT_CONTROL_H

// ROS includes
#include <ros/ros.h>
#include <geometry_msgs/QuaternionStamped.h>
#include <geometry_msgs/Vector3Stamped.h>
#include <sensor_msgs/NavSatFix.h>
#include <std_msgs/UInt8.h>
#include <std_msgs/Bool.h>

// DJI SDK includes
#include <dji_sdk/DroneTaskControl.h>
#include <dji_sdk/SDKControlAuthority.h>
#include <dji_sdk/QueryDroneVersion.h>
#include <dji_sdk/SetLocalPosRef.h>


#include <tf/tf.h>
#include <sensor_msgs/Joy.h>

#define C_EARTH (double)6378137.0
#define C_PI (double)3.141592653589793
#define DEG2RAD(DEG) ((DEG) * ((C_PI) / (180.0)))

/*!
 * @brief a bare bone state machine to track the stage of the mission
 */
class Mission
{
public:
  // The basic state transition flow is:
  // 0---> 1 ---> 2 ---> ... ---> N ---> 0
  // where state 0 means the mission is note started
  // and each state i is for the process of moving to a target point.
  int state;

  int inbound_counter;
  int outbound_counter;
  int break_counter;
  int touchdown_counter;
  int hold_counter;
  int wait_counter;
  int up_counter;

  double t,dt,land_t;
  double grav,d,theta,phi,nd,ntheta,nphi;
  double v, acc_land, init_vel_land;
  double t_fac;
  bool land_flag,arm,icp_pp;


  float target_offset_x;
  float target_offset_y;
  float target_offset_z;
  float target_yaw;
  double x_vel, y_vel, z_vel,z_vel_current;
  sensor_msgs::NavSatFix start_gps_location;
  geometry_msgs::Point start_local_position;

  bool finished;

  Mission() : state(0), inbound_counter(0), outbound_counter(0), break_counter(0),land_flag(false),land_t(0),touchdown_counter(0),acc_land(0),t_fac(3),init_vel_land(0.0),arm(false),icp_pp(false),wait_counter(0),up_counter(0),
              target_offset_x(0.0), target_offset_y(0.0), target_offset_z(0.0),x_vel(0.0),y_vel(0.0),z_vel(0.0),d(0.01),theta(0.0),phi(0.0),nd(0.0),ntheta(0.0),nphi(0.0),v(0.0),z_vel_current(0.0),grav(0.0),dt(0.01),t(0.00),hold_counter(0),
              finished(false)
  {
  }

  void step();

  void setTarget(float x, float y, float z, float yaw)
  {
    target_offset_x = x;
    target_offset_y = y;
    target_offset_z = z;
    target_yaw      = yaw;
  }

  void reset()
  {
    inbound_counter = 0;
    outbound_counter = 0;
    break_counter = 0;
    finished = false;
  }

  bool set_mission(double d_, double theta_, double phi_, double t_fac_);
  void hop_fill_vel(double Vx, double Vy, double Vz, double yaw);
  void hop_ex();
};

void localOffsetFromGpsOffset(geometry_msgs::Vector3&  deltaNed,
                         sensor_msgs::NavSatFix& target,
                         sensor_msgs::NavSatFix& origin);

geometry_msgs::Vector3 toEulerAngle(geometry_msgs::Quaternion quat);

void display_mode_callback(const std_msgs::UInt8::ConstPtr& msg);

void flight_status_callback(const std_msgs::UInt8::ConstPtr& msg);

void gps_callback(const sensor_msgs::NavSatFix::ConstPtr& msg);

void attitude_callback(const geometry_msgs::QuaternionStamped::ConstPtr& msg);

void getVelocity_callback(const geometry_msgs::Vector3Stamped& vel_from_sdk);
void local_position_callback(const geometry_msgs::PointStamped::ConstPtr& msg);
void icp_pp_status_callback(const std_msgs::Bool& status);

bool takeoff_land(int task);

bool obtain_control();

bool is_M100();

bool monitoredTakeoff();

bool M100monitoredTakeoff();

bool set_local_position();
bool arm_motors(void);
bool disarm_motors(void);

bool set_next_hop();

#endif // DEMO_FLIGHT_CONTROL_H
