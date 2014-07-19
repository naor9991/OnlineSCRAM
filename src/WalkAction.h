/*
 * WalkAction.cpp
 *
 *  Created on: Jul 14, 2014
 *      Author: naor
 */

#ifndef WALKACTION_CPP_
#define WALKACTION_CPP_

#include <tf/transform_datatypes.h>
#include <tf/transform_listener.h>

#include "geometry_msgs/Twist.h"
#include <sensor_msgs/LaserScan.h>

#include <ros/ros.h>
#include <ros/time.h>
#include <move_base_msgs/MoveBaseAction.h>
#include <actionlib/client/simple_action_client.h>
#include <actionlib/server/simple_action_server.h>
#include <online_scram/WalkAction.h>

#include <boost/algorithm/string.hpp>
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>

using namespace std;
using namespace boost;

#define MAX_LINEAR_VEL 0.7
#define MAX_ANGULAR_VEL 3.14
#define NEGLIBLE_DISTANCE 0.05 //5 centimeters

typedef actionlib::SimpleActionClient<move_base_msgs::MoveBaseAction> MoveBaseClient;

bool obstacleFound = false;

void readSensorCallback(const sensor_msgs::LaserScan::ConstPtr &sensor_msg);

class WalkAction
{
protected:

  ros::NodeHandle nh_;
  // NodeHandle instance must be created before this line. Otherwise strange error may occur.
  actionlib::SimpleActionServer<online_scram::WalkAction> as_;
  std::string action_name_;
  // create messages that are used to published feedback/result
  online_scram::WalkFeedback feedback_;
  online_scram::WalkResult result_;
  int robot_id_;
  MoveBaseClient *moveBaseClient_;

  ros::Publisher cmdVelPublisher_;
  ros::Subscriber baseScanSubscriber_;

public:

  WalkAction(std::string name, int robot_id) :
    as_(nh_, name, false),
    action_name_(name), robot_id_(robot_id)
  {

	//register the goal and feeback callbacks
	as_.registerGoalCallback(boost::bind(&WalkAction::goalCB, this));
	// as_.registerPreemptCallback(boost::bind(&AveragingAction::preemptCB, this));

	string cmd_vel_str = "/robot_";
	cmd_vel_str += boost::lexical_cast<string>(robot_id);
	cmd_vel_str += "/cmd_vel";
	cmdVelPublisher_ = nh_.advertise<geometry_msgs::Twist>(cmd_vel_str, 10);

	string move_base_str = "/robot_";
	move_base_str += boost::lexical_cast<string>(robot_id);
	move_base_str += "/move_base";
	ROS_INFO("move_base_str is %s",move_base_str.c_str());
	moveBaseClient_ = new MoveBaseClient(move_base_str, true);
	// Wait for the action server to become available
	ROS_INFO("Waiting for the move_base action server");
	moveBaseClient_->waitForServer(ros::Duration(5));

	// subscribe to robot's laser scan topic "base_scan"
	string laser_scan_topic_name = "base_scan";

	baseScanSubscriber_ = nh_.subscribe<sensor_msgs::LaserScan>(laser_scan_topic_name, 1,
            &WalkAction::readSensorCallback);

	// ros::Subscriber laser_sub = n.subscribe("scan", 100, laserCallback);

	// ros::Subscriber ranger_sub = n.subscribe("range", 100, rangeCallback);

    as_.start();
  }

  ~WalkAction(void)
  {
	  delete moveBaseClient_;
  }

  void goalCB()
  {
	// online_scram::WalkGoalConstPtr
	online_scram::WalkGoalConstPtr goal = as_.acceptNewGoal();
	// helper variables
    bool success = true;
    string tf_prefix;
    nh_.getParam("tf_prefix", tf_prefix);

    // push_back the seeds for the fibonacci sequence
    feedback_.is_waiting = false;

  	tf::TransformListener listener;
  	string this_robot_frame = tf::resolve(tf_prefix, "base_footprint");

  	// wait to have some transform before start
  	listener.waitForTransform("/map", this_robot_frame, ros::Time(0), ros::Duration(10.0));
  	/*
  	tf::StampedTransform transform;
  	try {
		listener.lookupTransform("/map", this_robot_frame, ros::Time(0), transform);
	}
	catch (tf::TransformException &ex) {
		ROS_ERROR("%s",ex.what());
	}

	ROS_INFO("Robot %d with goal %d,%d", robot_id_, goal->x, goal->y);
	double theta = atan2(double(goal->y - transform.getOrigin().y()),
			double(goal->x - transform.getOrigin().x()));

	move_base_msgs::MoveBaseGoal moveBaseGoal;
	moveBaseGoal.target_pose.header.frame_id = "map";
	moveBaseGoal.target_pose.header.stamp = ros::Time::now();

	moveBaseGoal.target_pose.pose.position.x = transform.getOrigin().x();
	moveBaseGoal.target_pose.pose.position.y = transform.getOrigin().y();

	// Convert the Euler angle to quaternion
	tf::Quaternion quaternion;
	quaternion = tf::createQuaternionFromYaw(theta);

	geometry_msgs::Quaternion qMsg;
	tf::quaternionTFToMsg(quaternion, qMsg);
	moveBaseGoal.target_pose.pose.orientation = qMsg;

	ros::Rate loopRate(10);
	ROS_INFO("Sending goal to change angle with x=%f,y=%f", transform.getOrigin().x(),
			transform.getOrigin().y());
	moveBaseClient_->sendGoal(moveBaseGoal);
	ROS_INFO("Waiting for result");
	ros::Duration(1).sleep();
	ROS_INFO("state is %s", moveBaseClient_->getState().toString().c_str());
	moveBaseClient_->waitForResult();
	ROS_INFO("result reached");
	*/
  	ros::Rate fasterLoopRate(20);
  	while (ros::ok()) {
  		as_.publishFeedback(feedback_);

        // check that preempt has not been requested by the client
        if (as_.isPreemptRequested() || !ros::ok())
        {
          ROS_INFO("%s: Preempted", action_name_.c_str());
          // set the action state to preempted
          as_.setPreempted();
          success = false;
          break;
        }

  		tf::StampedTransform transform;

  		try {
  			listener.lookupTransform("/map", this_robot_frame, ros::Time(0), transform);
  		}
  		catch (tf::TransformException &ex) {
  			ROS_ERROR("%s",ex.what());
  		}
  		/*
  		double x_diff = transform.getOrigin().x() - goal->x;
  		double y_diff = transform.getOrigin().y() - goal->y;
  		*/
  		double x_diff = goal->x - transform.getOrigin().x();
  		double y_diff = goal->y - transform.getOrigin().y();
  		double dist = sqrt(pow(x_diff, 2) + pow(y_diff, 2));
  		double theta =  atan2(y_diff,x_diff) - tf::getYaw(transform.getRotation());
  		x_diff = dist*cos(theta);
  		y_diff = dist*sin(theta);

		geometry_msgs::Twist vel_msg;

		if (dist > NEGLIBLE_DISTANCE) {
			if (obstacleFound == true) {
				ROS_WARN("ROBOT %d is waiting", robot_id_);
				feedback_.is_waiting = true;
			}
			else {
				vel_msg.linear.x = min(dist, MAX_LINEAR_VEL);
				vel_msg.angular.z = min(4 * atan2(y_diff,x_diff), MAX_ANGULAR_VEL);
				feedback_.is_waiting = false;
			}
		}
		else {
			as_.setSucceeded(result_);
			break;
		}
		cmdVelPublisher_.publish(vel_msg);
  		/*
  		ROS_INFO("Robot %d is at yaw %f and should",
  				robot_id_, tf::getYaw(transform.getRotation()));
  		double theta =  atan2(y_diff,x_diff) - tf::getYaw(transform.getRotation());

  		geometry_msgs::Twist vel_msg;
  		vel_msg.linear.z = 0;
  		/*
  		if ((theta < 5*M_PI/180) && (theta > -5*M_PI/180)) {
  			vel_msg.angular.z = 0;
  		}
  		else {
  			vel_msg.angular.z = max(theta, double(MAX_ANGULAR_VEL));
  		}
		*/
  		/*
  		if (dist < NEGLIBLE_DISTANCE) {
  			result_.total_dishes_cleaned = 3;
  			ROS_INFO("%s: Succeeded", action_name_.c_str());
  			vel_msg.linear.x = 0;
  			vel_msg.linear.y = 0;
  			cmdVelPublisher_.publish(vel_msg);
  			// set the action state to succeeded
  			as_.setSucceeded(result_);
  			success = true;
  			break;
  		}
  		else {
  			double factor = fabs(y_diff)/fabs(x_diff);
  			double siz = min(dist,double(MAX_LINEAR_VEL))/sqrt(1+pow(factor,2));
  			int x_sign = (x_diff>0)?(-1):(1);
  			int y_sign = (y_diff>0)?(-1):(1);
  			// vel_msg.linear.x = min(dist, double(MAX_LINEAR_VEL));
  			vel_msg.linear.x = x_sign*siz;
  			vel_msg.linear.y = y_sign*factor*siz;
  			cmdVelPublisher_.publish(vel_msg);
  		}
  		*/

  		ros::spinOnce();
  		fasterLoopRate.sleep();
  	}
  }

  static void readSensorCallback(const sensor_msgs::LaserScan::ConstPtr &sensor_msg)
  {
      int arraySize = (sensor_msg->angle_max - sensor_msg->angle_min) /
              sensor_msg->angle_increment;

      bool isObstacle = false;

      for (int i = 0; i < arraySize; i++) {
          if ((sensor_msg->ranges[i] > sensor_msg->range_min) &&
        		  (sensor_msg->ranges[i] < sensor_msg->range_max) &&
        		  (sensor_msg->ranges[i] < 0.5)) {
              isObstacle = true;
          }
      }


      if (isObstacle)
      {
          obstacleFound = true;
      } else {
          obstacleFound = false;
      }
  }

void executeCB2(const online_scram::WalkGoalConstPtr &goal)
  {
	string tf_prefix;
	nh_.getParam("tf_prefix", tf_prefix);

	// push_back the seeds for the fibonacci sequence
	feedback_.is_waiting = false;

	tf::TransformListener listener;
	string this_robot_frame = tf::resolve(tf_prefix, "base_footprint");

	string target_str = "/robot_";
	target_str += boost::lexical_cast<string>(goal->t);
	string target_frame = tf::resolve(target_str, "base_footprint");

	// wait to have some transform before start
	listener.waitForTransform(this_robot_frame, target_frame, ros::Time(0), ros::Duration(10.0));

	// make the frequency larger to hadle with obstacles
	ros::Rate fasterLoopRate(10);
	while (ros::ok()) {

		// check that preempt has not been requested by the client
		if (as_.isPreemptRequested() || !ros::ok())
		{
		  ROS_INFO("%s: Preempted", action_name_.c_str());
		  // set the action state to preempted
		  as_.setPreempted();
		  break;
		}

		tf::StampedTransform transform;

		try {
			listener.lookupTransform(this_robot_frame, target_frame, ros::Time(0), transform);
		}
		catch (tf::TransformException &ex) {
			ROS_ERROR("%s",ex.what());
		}

		double dist = sqrt(pow(transform.getOrigin().x(), 2) +
				pow(transform.getOrigin().y(), 2));
		geometry_msgs::Twist vel_msg;

		if (dist < NEGLIBLE_DISTANCE) {
			as_.setSucceeded(result_);
		}
		else {
			vel_msg.linear.x = min(dist, MAX_LINEAR_VEL);
			vel_msg.angular.z = min(4 * atan2(transform.getOrigin().y(),
										  transform.getOrigin().x()), MAX_ANGULAR_VEL);
		}
		cmdVelPublisher_.publish(vel_msg);

		ros::spinOnce();
		fasterLoopRate.sleep();
	}
  };

};
#endif /* WALKACTION_CPP_ */
