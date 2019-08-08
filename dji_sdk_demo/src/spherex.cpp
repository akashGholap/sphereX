#include "dji_sdk_demo/demo_flight_control.h"
#include "dji_sdk/dji_sdk.h"
#include <dji_sdk/DroneArmControl.h>
#include "std_msgs/Bool.h"
//#include <Eigen>
//#include <dji_sdk_demo/spacetrex_kalman_filter.h>
#include <iostream>
//#include <future>
#include <cstdio>
#include <dlib/optimization.h>
//using namespace kf;
//#include "dji_sdk_demo/spherex.h"
#include "math.h"

const float deg2rad = C_PI/180.0;
const float rad2deg = 180.0/C_PI;

//ros::ServiceClient set_local_pos_reference;
ros::ServiceClient sdk_ctrl_authority_service;
ros::ServiceClient drone_task_service;
ros::ServiceClient drone_arm_service;
ros::ServiceClient query_version_service;
//ros::ServiceClient query_version_service;

uint8_t flight_status = 255;
ros::Publisher ctrlVelYawratePub;
ros::Publisher hopStatusPub;
geometry_msgs::Quaternion current_atti;
std::ofstream file1;
std::ofstream file2;

Mission hop;

int main(int argc, char** argv)
{
  ros::init(argc, argv, "spherex");
  ros::NodeHandle nh;
  ros::Subscriber attitudeSub = nh.subscribe("dji_sdk/attitude", 50, &attitude_callback);
  ros::Subscriber getVelocity = nh.subscribe("dji_sdk/velocity" ,100, &getVelocity_callback);
  ros::Subscriber flightStatusSub = nh.subscribe("dji_sdk/flight_status", 10, &flight_status_callback);
  //ros::Subscriber lidarProcessStatusSub = nh.subscribe("spherex/lidar_process_status", 100, &lidar_process_status_callback);ls

  ctrlVelYawratePub = nh.advertise<sensor_msgs::Joy>("dji_sdk/flight_control_setpoint_ENUvelocity_yawrate", 50);
  hopStatusPub = nh.advertise<std_msgs::Bool>("spherex/hopStatus",100);
  //note
  //make a hop status advertiser


  sdk_ctrl_authority_service = nh.serviceClient<dji_sdk::SDKControlAuthority> ("dji_sdk/sdk_control_authority");
  drone_task_service         = nh.serviceClient<dji_sdk::DroneTaskControl>("dji_sdk/drone_task_control");
  drone_arm_service          = nh.serviceClient<dji_sdk::DroneArmControl>("dji_sdk/drone_arm_control");
  query_version_service      = nh.serviceClient<dji_sdk::QueryDroneVersion>("dji_sdk/query_drone_version");

  file1.open("velocity_from_sdk.csv");
  file2.open("velocity_published.csv");
  bool obtain_control_flag = obtain_control();
  if(obtain_control_flag)
  {
  ROS_INFO("Obtain Control Success");
  }
  else ROS_INFO("Obtain Control Failed");

  double g, d, theta, phi, t_fac;
  bool hopped = true;

  while(ros::ok())
  {
    if(hopped)
    {
      std::cout<<"Please Enter d, theta, phi, and landing factor; g is = 1.62";
      std::cin>>d>>theta>>phi>>t_fac;
      hop.set_mission(d,theta,phi,t_fac);
      hopped = false;
    }
    hopped  =  hop.finished;
    hop_status_pub();
    ros::spinOnce();
  }

}

void Mission::hop_ex()
{
        double tc = hop.t;
        double z_vel_c = hop.z_vel-1.62*tc;
        if(z_vel_c<=(-0.8)*hop.z_vel && (!hop.land_flag))
        {
          hop.land_flag = true;
          hop.acc_land = z_vel_c/hop.t_fac;
          hop.init_vel_land = z_vel_c;
          //hop.land_flag = false;
          ROS_INFO("landing acceleration %f", hop.acc_land);
        }

        if(hop.land_flag)
        {
          hop.land_t = hop.land_t + 0.01;
          double hop_land_vel = hop.init_vel_land - hop.acc_land*hop.land_t;
          if(hop_land_vel >= -0.25)
          {
            hop.touchdown_counter++;
            if(hop.touchdown_counter >= 10)
            {
              hop_fill_vel(0,0,0,1.57);
              hop.finished = true;
              ROS_INFO("SphereX Landed");
              disarm_motors();
            }
            else
            {
              hop_fill_vel(0,0,-0.20,1.57);
               hop.finished = false;
               ROS_INFO("In touchdown");   //return true;
            }
          }
          else
          {
            hop_fill_vel(0,0, hop_land_vel,1.57);
            ROS_INFO("%lf, %lf, %lf, %f", 0, 0 , hop_land_vel, 0);
            ROS_INFO("Pre touchdown");
          }
        }
        else
        {
          if(hop.check_yaw())
          {
            hop_fill_vel(hop.x_vel,hop.y_vel,z_vel_c,1.57);
            ROS_INFO("%lf, %lf, %lf, %f", hop.x_vel,hop.y_vel,z_vel_c, hop.yaw_rate);
          }
          else
          {
          hop_fill_vel(hop.x_vel,hop.y_vel,z_vel_c,1.57);
          ROS_INFO("%lf, %lf, %lf, %f", hop.x_vel,hop.y_vel,z_vel_c, 0);
          }

          hop.land_t = 0;
          hop.finished = false;
        }
        std_msgs::Bool hopStatus;
        hopStatus.data = hop.finished;
        hopStatusPub.publish(hopStatus);
}

bool Mission::check_yaw(void)
{
  float yawThresholdInDeg   = 2;
  double yawDesiredRad     = 0;
  double yawThresholdInRad = deg2rad * yawThresholdInDeg;
  double yawInRad          = toEulerAngle(current_atti).z;
  if((yawInRad - yawDesiredRad) < yawThresholdInRad)
  {
    return 1;
  }
  else return 0;

}
geometry_msgs::Vector3 toEulerAngle(geometry_msgs::Quaternion quat)
{
  geometry_msgs::Vector3 ans;

  tf::Matrix3x3 R_FLU2ENU(tf::Quaternion(quat.x, quat.y, quat.z, quat.w));
  R_FLU2ENU.getRPY(ans.x, ans.y, ans.z);
  return ans;
}
void Mission::hop_fill_vel(double Vx, double Vy, double Vz, double yaw)
{

  sensor_msgs::Joy controlVelYawRate;
  controlVelYawRate.axes.push_back(Vx);
  controlVelYawRate.axes.push_back(Vy);
  controlVelYawRate.axes.push_back(Vz);
  controlVelYawRate.axes.push_back(yaw);
  ctrlVelYawratePub.publish(controlVelYawRate);
  ROS_INFO("PUBLISHING VELOCITY");
  std::stringstream ss;
  ss<<Vx<<","<<Vy<<","<<Vz<<"\n";
  file2 << ss.str();
}

bool Mission::set_mission(double d_, double theta_, double phi_, double t_fac_)
{
    dt = 0.016;
    grav = 1.62;
    d = d_;
    theta = theta_;
    phi = phi_;
    t = 0.00;
    t_fac = t_fac_;
    acc_land = 1.62;
    finished =false;
    touchdown_counter = 0;
    hold_counter = 0;
    wait_counter = 0;
    up_counter = 0;
    land_flag = false;
    arm = false;
    double theta_rad = (theta*3.14)/180;
    double phi_rad = (phi*3.14)/180;
    ROS_INFO("Calculating the intial velocity vector");
    v = sqrt((d*grav)/sin(2*theta_rad));
    x_vel = v*cos(theta_rad)*sin(phi_rad);
    y_vel = v*cos(theta_rad)*cos(phi_rad);
    z_vel = v*sin(theta_rad);
    t_est = 1.8*v*sin(theta_rad)/grav;
    yaw_rate = phi/t_est;
    ROS_INFO("Not Stuck");
    return true;
}


void getVelocity_callback(const geometry_msgs::Vector3Stamped& vel_from_sdk) // prototyped in the flight control header
{
  ROS_INFO("In velocity callback");
  hop.hold_counter++;
  if(hop.hold_counter==100)
  {
    hop.arm = arm_motors();
  }
  if(!hop.arm && (hop.hold_counter==200))
  {
    hop.arm = arm_motors();
    if(!hop.arm)
    {
      ROS_INFO("SphereX is not arming");
    }
  }
  else if(hop.arm && (hop.hold_counter==200))
  {
    ROS_INFO("SphereX Arming Successful");
  }
  if(hop.hold_counter>=400)
  {
    if(hop.wait_counter<=100)
    {
      hop.hop_fill_vel(0,0,0,0);
      hop.wait_counter++;
      hop.finished = false;
    }
    else if(hop.wait_counter>100&&hop.up_counter<=10)
    {
      hop.hop_fill_vel(0,0,2.0,0);
      hop.up_counter++;
      hop.finished = false;
    }
    else
    {
      hop.t = hop.t + hop.dt ;
      hop.hop_ex();
    }

  }
  std::stringstream ss;
  ss<<vel_from_sdk.vector.x<<","<<vel_from_sdk.vector.y<<","<<vel_from_sdk.vector.z<<"\n";
  file1 << ss.str();
}

bool obtain_control()
{
  dji_sdk::SDKControlAuthority authority;
  authority.request.control_enable=1;
  sdk_ctrl_authority_service.call(authority);

  if(!authority.response.result)
  {
    ROS_ERROR("obtain control failed!");
    return false;
  }

  return true;
}
void hop_status_pub()
{
  std_msgs::Bool hopStatus;
  hopStatus.data = hop.finished;
  hopStatusPub.publish(hopStatus);
}
bool arm_motors()
{
  dji_sdk::DroneArmControl droneArmControl;
  droneArmControl.request.arm = 1;
  drone_arm_service.call(droneArmControl);
  if(!droneArmControl.response.result)
  {
    ROS_ERROR("Arming Failed");
      return false;
  }
  return true;
}

bool disarm_motors()
{
  dji_sdk::DroneArmControl droneArmControl;
  droneArmControl.request.arm = 0;
  drone_arm_service.call(droneArmControl);
  if(!droneArmControl.response.result)
  {
    ROS_ERROR("Disarming Failed");
      return false;
  }
  return true;
}
bool is_M100()
{
  dji_sdk::QueryDroneVersion query;
  query_version_service.call(query);

  if(query.response.version == DJISDK::DroneFirmwareVersion::M100_31)
  {
    return true;
  }

  return false;
}
void flight_status_callback(const std_msgs::UInt8::ConstPtr& msg)
{
  flight_status = msg->data;
}
void attitude_callback(const geometry_msgs::QuaternionStamped::ConstPtr& msg)
{
  current_atti = msg->quaternion;
}
