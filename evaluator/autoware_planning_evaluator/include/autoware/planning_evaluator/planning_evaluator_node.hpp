// Copyright 2021 Tier IV, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef AUTOWARE__PLANNING_EVALUATOR__PLANNING_EVALUATOR_NODE_HPP_
#define AUTOWARE__PLANNING_EVALUATOR__PLANNING_EVALUATOR_NODE_HPP_

#include "autoware/planning_evaluator/metrics_calculator.hpp"
#include "autoware/planning_evaluator/stat.hpp"
#include "rclcpp/rclcpp.hpp"
#include "tf2_ros/buffer.h"
#include "tf2_ros/transform_listener.h"

#include "autoware_perception_msgs/msg/predicted_objects.hpp"
#include "autoware_planning_msgs/msg/pose_with_uuid_stamped.hpp"
#include "autoware_planning_msgs/msg/trajectory.hpp"
#include "autoware_planning_msgs/msg/trajectory_point.hpp"
#include "diagnostic_msgs/msg/diagnostic_array.hpp"
#include "nav_msgs/msg/odometry.hpp"
#include <autoware_planning_msgs/msg/lanelet_route.hpp>
#include <diagnostic_msgs/msg/detail/diagnostic_status__struct.hpp>

#include <array>
#include <deque>
#include <memory>
#include <string>
#include <utility>
#include <vector>
namespace planning_diagnostics
{
using autoware_perception_msgs::msg::PredictedObjects;
using autoware_planning_msgs::msg::PoseWithUuidStamped;
using autoware_planning_msgs::msg::Trajectory;
using autoware_planning_msgs::msg::TrajectoryPoint;
using diagnostic_msgs::msg::DiagnosticArray;
using diagnostic_msgs::msg::DiagnosticStatus;
using nav_msgs::msg::Odometry;
/**
 * @brief Node for planning evaluation
 */
class PlanningEvaluatorNode : public rclcpp::Node
{
public:
  explicit PlanningEvaluatorNode(const rclcpp::NodeOptions & node_options);
  ~PlanningEvaluatorNode();

  /**
   * @brief callback on receiving an odometry
   * @param [in] odometry_msg received odometry message
   */
  void onOdometry(const Odometry::ConstSharedPtr odometry_msg);

  /**
   * @brief callback on receiving a trajectory
   * @param [in] traj_msg received trajectory message
   */
  void onTrajectory(const Trajectory::ConstSharedPtr traj_msg);

  /**
   * @brief callback on receiving a reference trajectory
   * @param [in] traj_msg received trajectory message
   */
  void onReferenceTrajectory(const Trajectory::ConstSharedPtr traj_msg);

  /**
   * @brief callback on receiving a dynamic objects array
   * @param [in] objects_msg received dynamic object array message
   */
  void onObjects(const PredictedObjects::ConstSharedPtr objects_msg);

  /**
   * @brief callback on receiving a modified goal
   * @param [in] modified_goal_msg received modified goal message
   */
  void onModifiedGoal(const PoseWithUuidStamped::ConstSharedPtr modified_goal_msg);

  /**
   * @brief obtain diagnostics information
   */
  void onDiagnostics(const DiagnosticArray::ConstSharedPtr diag_msg);

  /**
   * @brief publish the given metric statistic
   */
  DiagnosticStatus generateDiagnosticStatus(
    const Metric & metric, const Stat<double> & metric_stat) const;

  /**
   * @brief publish current ego lane info
   */
  DiagnosticStatus generateDiagnosticEvaluationStatus(const DiagnosticStatus & diag);

  /**
   * @brief publish current ego lane info
   */
  DiagnosticStatus generateLaneletDiagnosticStatus(const Odometry::ConstSharedPtr ego_state_ptr);

  /**
   * @brief publish current ego kinematic state
   */
  DiagnosticStatus generateKinematicStateDiagnosticStatus(
    const AccelWithCovarianceStamped & accel_stamped, const Odometry::ConstSharedPtr ego_state_ptr);

private:
  static bool isFinite(const TrajectoryPoint & p);

  /**
   * @brief update route handler data
   */
  void getRouteData();

  /**
   * @brief fetch data and publish diagnostics
   */
  void onTimer();

  /**
   * @brief fetch topic data
   */
  void fetchData();
  // The diagnostics cycle is faster than timer, and each node publishes diagnostic separately.
  // takeData() in onTimer() with a polling subscriber will miss a topic, so save all topics with
  // onDiagnostics().
  rclcpp::Subscription<DiagnosticArray>::SharedPtr planning_diag_sub_;

  // ROS
  rclcpp::Subscription<Trajectory>::SharedPtr traj_sub_;
  rclcpp::Subscription<Trajectory>::SharedPtr ref_sub_;
  rclcpp::Subscription<PredictedObjects>::SharedPtr objects_sub_;
  rclcpp::Subscription<PoseWithUuidStamped>::SharedPtr modified_goal_sub_;
  rclcpp::Subscription<Odometry>::SharedPtr odom_sub_;

  rclcpp::Publisher<DiagnosticArray>::SharedPtr metrics_pub_;
  std::shared_ptr<tf2_ros::TransformListener> transform_listener_{nullptr};
  std::unique_ptr<tf2_ros::Buffer> tf_buffer_;

  // Parameters
  std::string output_file_str_;
  std::string ego_frame_str_;

  // Calculator
  MetricsCalculator metrics_calculator_;
  // Metrics
  std::vector<Metric> metrics_;
  std::deque<rclcpp::Time> stamps_;
  std::array<std::deque<Stat<double>>, static_cast<size_t>(Metric::SIZE)> metric_stats_;

  rclcpp::TimerBase::SharedPtr timer_;
  // queue for diagnostics and time stamp
  std::deque<std::pair<DiagnosticStatus, rclcpp::Time>> diag_queue_;
  const std::vector<std::string> target_functions_ = {"obstacle_cruise_planner"};
  std::optional<AccelWithCovarianceStamped> prev_acc_stamped_{std::nullopt};
};
}  // namespace planning_diagnostics

#endif  // AUTOWARE__PLANNING_EVALUATOR__PLANNING_EVALUATOR_NODE_HPP_
