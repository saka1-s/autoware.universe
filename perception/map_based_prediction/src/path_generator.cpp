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

#include "map_based_prediction/path_generator.hpp"

#include <interpolation/linear_interpolation.hpp>
#include <motion_utils/trajectory/trajectory.hpp>
#include <tier4_autoware_utils/geometry/geometry.hpp>

#include <algorithm>

namespace map_based_prediction
{
PathGenerator::PathGenerator(
  const double time_horizon, const double lateral_time_horizon, const double sampling_time_interval,
  const double min_crosswalk_user_velocity)
: time_horizon_(time_horizon),
  lateral_time_horizon_(lateral_time_horizon),
  sampling_time_interval_(sampling_time_interval),
  min_crosswalk_user_velocity_(min_crosswalk_user_velocity)
{
}

PredictedPath PathGenerator::generatePathForNonVehicleObject(const TrackedObject & object)
{
  return generateStraightPath(object);
}

PredictedPath PathGenerator::generatePathToTargetPoint(
  const TrackedObject & object, const Eigen::Vector2d & point) const
{
  PredictedPath predicted_path{};
  const double ep = 0.001;

  const auto & obj_pos = object.kinematics.pose_with_covariance.pose.position;
  const auto & obj_vel = object.kinematics.twist_with_covariance.twist.linear;

  const Eigen::Vector2d pedestrian_to_entry_point(point.x() - obj_pos.x, point.y() - obj_pos.y);
  const auto velocity = std::max(std::hypot(obj_vel.x, obj_vel.y), min_crosswalk_user_velocity_);
  const auto arrival_time = pedestrian_to_entry_point.norm() / velocity;

  for (double dt = 0.0; dt < arrival_time + ep; dt += sampling_time_interval_) {
    geometry_msgs::msg::Pose world_frame_pose;
    world_frame_pose.position.x =
      obj_pos.x + velocity * pedestrian_to_entry_point.normalized().x() * dt;
    world_frame_pose.position.y =
      obj_pos.y + velocity * pedestrian_to_entry_point.normalized().y() * dt;
    world_frame_pose.position.z = obj_pos.z;
    world_frame_pose.orientation = object.kinematics.pose_with_covariance.pose.orientation;
    predicted_path.path.push_back(world_frame_pose);
    if (predicted_path.path.size() >= predicted_path.path.max_size()) {
      break;
    }
  }

  predicted_path.confidence = 1.0;
  predicted_path.time_step = rclcpp::Duration::from_seconds(sampling_time_interval_);

  return predicted_path;
}

PredictedPath PathGenerator::generatePathForCrosswalkUser(
  const TrackedObject & object, const CrosswalkEdgePoints & reachable_crosswalk) const
{
  PredictedPath predicted_path{};
  const double ep = 0.001;

  const auto & obj_pos = object.kinematics.pose_with_covariance.pose.position;
  const auto & obj_vel = object.kinematics.twist_with_covariance.twist.linear;

  const Eigen::Vector2d pedestrian_to_entry_point(
    reachable_crosswalk.front_center_point.x() - obj_pos.x,
    reachable_crosswalk.front_center_point.y() - obj_pos.y);
  const Eigen::Vector2d entry_to_exit_point(
    reachable_crosswalk.back_center_point.x() - reachable_crosswalk.front_center_point.x(),
    reachable_crosswalk.back_center_point.y() - reachable_crosswalk.front_center_point.y());
  const auto velocity = std::max(std::hypot(obj_vel.x, obj_vel.y), min_crosswalk_user_velocity_);
  const auto arrival_time = pedestrian_to_entry_point.norm() / velocity;

  for (double dt = 0.0; dt < time_horizon_ + ep; dt += sampling_time_interval_) {
    geometry_msgs::msg::Pose world_frame_pose;
    if (dt < arrival_time) {
      world_frame_pose.position.x =
        obj_pos.x + velocity * pedestrian_to_entry_point.normalized().x() * dt;
      world_frame_pose.position.y =
        obj_pos.y + velocity * pedestrian_to_entry_point.normalized().y() * dt;
      world_frame_pose.position.z = obj_pos.z;
      world_frame_pose.orientation = object.kinematics.pose_with_covariance.pose.orientation;
      predicted_path.path.push_back(world_frame_pose);
    } else {
      world_frame_pose.position.x =
        reachable_crosswalk.front_center_point.x() +
        velocity * entry_to_exit_point.normalized().x() * (dt - arrival_time);
      world_frame_pose.position.y =
        reachable_crosswalk.front_center_point.y() +
        velocity * entry_to_exit_point.normalized().y() * (dt - arrival_time);
      world_frame_pose.position.z = obj_pos.z;
      world_frame_pose.orientation = object.kinematics.pose_with_covariance.pose.orientation;
      predicted_path.path.push_back(world_frame_pose);
    }
    if (predicted_path.path.size() >= predicted_path.path.max_size()) {
      break;
    }
  }

  // calculate orientation of each point
  if (predicted_path.path.size() >= 2) {
    for (size_t i = 0; i < predicted_path.path.size() - 1; i++) {
      const auto yaw = tier4_autoware_utils::calcAzimuthAngle(
        predicted_path.path.at(i).position, predicted_path.path.at(i + 1).position);
      predicted_path.path.at(i).orientation = tier4_autoware_utils::createQuaternionFromYaw(yaw);
    }
    predicted_path.path.back().orientation =
      predicted_path.path.at(predicted_path.path.size() - 2).orientation;
  }

  predicted_path.confidence = 1.0;
  predicted_path.time_step = rclcpp::Duration::from_seconds(sampling_time_interval_);

  return predicted_path;
}

PredictedPath PathGenerator::generatePathForLowSpeedVehicle(const TrackedObject & object) const
{
  PredictedPath path;
  path.time_step = rclcpp::Duration::from_seconds(sampling_time_interval_);
  const double ep = 0.001;
  for (double dt = 0.0; dt < time_horizon_ + ep; dt += sampling_time_interval_) {
    path.path.push_back(object.kinematics.pose_with_covariance.pose);
  }

  return path;
}

PredictedPath PathGenerator::generatePathForOffLaneVehicle(const TrackedObject & object)
{
  return generateStraightPath(object);
}

PredictedPath PathGenerator::generatePathForOnLaneVehicle(
  const TrackedObject & object, const PosePath & ref_path, const double path_width)
{
  if (ref_path.size() < 2) {
    return generateStraightPath(object);
  }

  // if the object is moving backward, we generate a straight path
  if (object.kinematics.twist_with_covariance.twist.linear.x < 0.0) {
    return generateStraightPath(object);
  }

  // get object width
  double object_width = 5.0;  // a large number
  if (object.shape.type == autoware_auto_perception_msgs::msg::Shape::BOUNDING_BOX) {
    object_width = object.shape.dimensions.y;
  } else if (object.shape.type == autoware_auto_perception_msgs::msg::Shape::CYLINDER) {
    object_width = object.shape.dimensions.x;
  }
  // Calculate the backlash width, which represents the maximum distance the object can be biased
  // from the reference path
  constexpr double margin =
    0.5;  // Set a safety margin of 0.5m for the object to stay away from the edge of the lane
  double backlash_width = (path_width - object_width) / 2.0 - margin;
  backlash_width = std::max(backlash_width, 0.0);  // minimum is 0.0

  return generatePolynomialPath(object, ref_path, path_width, backlash_width);
}

PredictedPath PathGenerator::generateStraightPath(const TrackedObject & object) const
{
  const auto & object_pose = object.kinematics.pose_with_covariance.pose;
  const auto & object_twist = object.kinematics.twist_with_covariance.twist;
  const double ep = 0.001;
  const double duration = time_horizon_ + ep;

  PredictedPath path;
  path.time_step = rclcpp::Duration::from_seconds(sampling_time_interval_);
  path.path.reserve(static_cast<size_t>((duration) / sampling_time_interval_));
  for (double dt = 0.0; dt < duration; dt += sampling_time_interval_) {
    const auto future_obj_pose = tier4_autoware_utils::calcOffsetPose(
      object_pose, object_twist.linear.x * dt, object_twist.linear.y * dt, 0.0);
    path.path.push_back(future_obj_pose);
  }

  return path;
}

PredictedPath PathGenerator::generatePolynomialPath(
  const TrackedObject & object, const PosePath & ref_path, const double path_width,
  const double backlash_width) const
{
  // Get current Frenet Point
  const double ref_path_len = motion_utils::calcArcLength(ref_path);
  const auto current_point = getFrenetPoint(object, ref_path.at(0));

  // Step1. Set Target Frenet Point
  // Note that we do not set position s,
  // since we don't know the target longitudinal position
  FrenetPoint terminal_point;
  terminal_point.s_vel = std::hypot(current_point.s_vel, current_point.d_vel);
  terminal_point.s_acc = 0.0;
  terminal_point.d = 0.0;
  terminal_point.d_vel = 0.0;
  terminal_point.d_acc = 0.0;

  // if the object is behind of the reference path adjust the lateral_time_horizon_ to reach the
  // start of the reference path
  double lateral_duration_adjusted = lateral_time_horizon_;
  if (current_point.s < 0.0) {
    const double distance_to_start = -current_point.s;
    const double duration_to_reach = distance_to_start / terminal_point.s_vel;
    lateral_duration_adjusted = std::max(lateral_duration_adjusted, duration_to_reach);
  }

  // calculate terminal d position, based on backlash width
  {
    if (backlash_width < 0.01 /*m*/) {
      // If the backlash width is less than 0.01m, do not consider the backlash width and reduce
      // calculation cost
      terminal_point.d = 0.0;
    } else {
      const double return_width = path_width / 2.0;  // [m]
      const double current_momentum_d =
        current_point.d + 0.5 * current_point.d_vel * lateral_duration_adjusted;
      const double momentum_d_abs = std::abs(current_momentum_d);

      if (momentum_d_abs < backlash_width) {
        // If the object momentum is within the backlash width, we set the target d position to the
        // current momentum
        terminal_point.d = current_momentum_d;
      } else if (
        momentum_d_abs >= backlash_width && momentum_d_abs < backlash_width + return_width) {
        // If the object momentum is within the return zone, we set the target d position close to
        // the zero gradually
        terminal_point.d =
          (backlash_width + return_width - momentum_d_abs) * backlash_width / return_width;
        terminal_point.d *= (current_momentum_d > 0) ? 1 : -1;
      } else {
        // If the object momentum is outside the backlash width + return zone, we set the target d
        // position to 0
        terminal_point.d = 0.0;
      }
    }
  }

  // Step2. Generate Predicted Path on a Frenet coordinate
  const auto frenet_predicted_path =
    generateFrenetPath(current_point, terminal_point, ref_path_len);

  // Step3. Interpolate Reference Path for converting predicted path coordinate
  const auto interpolated_ref_path = interpolateReferencePath(ref_path, frenet_predicted_path);

  if (frenet_predicted_path.size() < 2 || interpolated_ref_path.size() < 2) {
    return generateStraightPath(object);
  }

  // Step4. Convert predicted trajectory from Frenet to Cartesian coordinate
  return convertToPredictedPath(object, frenet_predicted_path, interpolated_ref_path);
}

FrenetPath PathGenerator::generateFrenetPath(
  const FrenetPoint & current_point, const FrenetPoint & target_point,
  const double max_length) const
{
  FrenetPath path;
  const double duration = time_horizon_;
  const double lateral_duration = lateral_time_horizon_;

  // Compute Lateral and Longitudinal Coefficients to generate the trajectory
  const Eigen::Vector3d lat_coeff =
    calcLatCoefficients(current_point, target_point, lateral_duration);
  const Eigen::Vector2d lon_coeff = calcLonCoefficients(current_point, target_point, duration);

  path.reserve(static_cast<size_t>(duration / sampling_time_interval_));
  for (double t = 0.0; t <= duration; t += sampling_time_interval_) {
    const auto t2 = t * t;
    const auto t3 = t2 * t;
    const auto t4 = t3 * t;
    const auto t5 = t4 * t;
    const double current_acc =
      0.0;  // Currently we assume the object is traveling at a constant speed
    const double d_next_ = current_point.d + current_point.d_vel * t + current_acc * 2.0 * t2 +
                           lat_coeff(0) * t3 + lat_coeff(1) * t4 + lat_coeff(2) * t5;
    // t > lateral_duration: target_point.d, else d_next_
    const double d_next = t > lateral_duration ? target_point.d : d_next_;
    const double s_next = current_point.s + current_point.s_vel * t + 2.0 * current_acc * t2 +
                          lon_coeff(0) * t3 + lon_coeff(1) * t4;
    if (s_next > max_length) {
      break;
    }

    // We assume the object is traveling at a constant speed along s direction
    FrenetPoint point;
    point.s = s_next;
    point.d = d_next;
    path.push_back(point);
  }

  return path;
}

Eigen::Vector3d PathGenerator::calcLatCoefficients(
  const FrenetPoint & current_point, const FrenetPoint & target_point, const double T) const
{
  // Lateral Path Calculation
  // Quintic polynomial for d
  // A = np.array([[T**3, T**4, T**5],
  //               [3 * T ** 2, 4 * T ** 3, 5 * T ** 4],
  //               [6 * T, 12 * T ** 2, 20 * T ** 3]])
  // A_inv = np.matrix([[10/(T**3), -4/(T**2), 1/(2*T)],
  //                    [-15/(T**4), 7/(T**3), -1/(T**2)],
  //                    [6/(T**5), -3/(T**4),  1/(2*T**3)]])
  // b = np.matrix([[xe - self.a0 - self.a1 * T - self.a2 * T**2],
  //                [vxe - self.a1 - 2 * self.a2 * T],
  //                [axe - 2 * self.a2]])
  const auto T2 = T * T;
  const auto T3 = T2 * T;
  const auto T4 = T3 * T;
  const auto T5 = T4 * T;

  Eigen::Matrix3d A_lat_inv;
  A_lat_inv << 10 / T3, -4 / T2, 1 / (2 * T), -15 / T4, 7 / T3, -1 / T2, 6 / T5, -3 / T4,
    1 / (2 * T3);
  Eigen::Vector3d b_lat;
  b_lat[0] = target_point.d - current_point.d - current_point.d_vel * T;
  b_lat[1] = target_point.d_vel - current_point.d_vel;
  b_lat[2] = target_point.d_acc;

  return A_lat_inv * b_lat;
}

Eigen::Vector2d PathGenerator::calcLonCoefficients(
  const FrenetPoint & current_point, const FrenetPoint & target_point, const double T) const
{
  // Longitudinal Path Calculation
  // Quadric polynomial
  // A_inv = np.matrix([[1/(T**2), -1/(3*T)],
  //                         [-1/(2*T**3), 1/(4*T**2)]])
  // b = np.matrix([[vxe - self.a1 - 2 * self.a2 * T],
  //               [axe - 2 * self.a2]])
  const auto T2 = T * T;
  const auto T3 = T2 * T;

  Eigen::Matrix2d A_lon_inv;
  A_lon_inv << 1 / T2, -1 / (3 * T), -1 / (2 * T3), 1 / (4 * T2);
  Eigen::Vector2d b_lon;
  b_lon[0] = target_point.s_vel - current_point.s_vel;
  b_lon[1] = 0.0;
  return A_lon_inv * b_lon;
}

std::vector<double> PathGenerator::interpolationLerp(
  const std::vector<double> & base_keys, const std::vector<double> & base_values,
  const std::vector<double> & query_keys) const
{
  // calculate linear interpolation
  // extrapolate the value if the query key is out of the base key range
  std::vector<double> query_values;
  size_t key_index = 0;
  double last_query_key = query_keys.at(0);
  for (const auto query_key : query_keys) {
    // search for the closest key index
    // if current query key is larger than the last query key, search base_keys increasing order
    if (query_key >= last_query_key) {
      while (base_keys.at(key_index + 1) < query_key) {
        if (key_index == base_keys.size() - 2) {
          break;
        }
        ++key_index;
      }
    } else {
      // if current query key is smaller than the last query key, search base_keys decreasing order
      while (base_keys.at(key_index) > query_key) {
        if (key_index == 0) {
          break;
        }
        --key_index;
      }
    }
    last_query_key = query_key;

    const double & src_val = base_values.at(key_index);
    const double & dst_val = base_values.at(key_index + 1);
    const double ratio = (query_key - base_keys.at(key_index)) /
                         (base_keys.at(key_index + 1) - base_keys.at(key_index));

    const double interpolated_val = src_val + (dst_val - src_val) * ratio;
    query_values.push_back(interpolated_val);
  }

  return query_values;
}

std::vector<tf2::Quaternion> PathGenerator::interpolationLerp(
  const std::vector<double> & base_keys, const std::vector<tf2::Quaternion> & base_values,
  const std::vector<double> & query_keys) const
{
  // calculate linear interpolation
  // extrapolate the value if the query key is out of the base key range
  std::vector<tf2::Quaternion> query_values;
  size_t key_index = 0;
  double last_query_key = query_keys.at(0);
  for (const auto query_key : query_keys) {
    // search for the closest key index
    // if current query key is larger than the last query key, search base_keys increasing order
    if (query_key >= last_query_key) {
      while (base_keys.at(key_index + 1) < query_key) {
        if (key_index == base_keys.size() - 2) {
          break;
        }
        ++key_index;
      }
    } else {
      // if current query key is smaller than the last query key, search base_keys decreasing order
      while (base_keys.at(key_index) > query_key) {
        if (key_index == 0) {
          break;
        }
        --key_index;
      }
    }
    last_query_key = query_key;

    const tf2::Quaternion & src_val = base_values.at(key_index);
    const tf2::Quaternion & dst_val = base_values.at(key_index + 1);
    const double ratio = (query_key - base_keys.at(key_index)) /
                         (base_keys.at(key_index + 1) - base_keys.at(key_index));

    // in case of extrapolation, export the nearest quaternion
    if (ratio < 0.0) {
      query_values.push_back(src_val);
      continue;
    }
    if (ratio > 1.0) {
      query_values.push_back(dst_val);
      continue;
    }
    const auto interpolated_quat = tf2::slerp(src_val, dst_val, ratio);
    query_values.push_back(interpolated_quat);
  }

  return query_values;
}

PosePath PathGenerator::interpolateReferencePath(
  const PosePath & base_path, const FrenetPath & frenet_predicted_path) const
{
  PosePath interpolated_path;
  const size_t interpolate_num = frenet_predicted_path.size();
  if (interpolate_num < 2) {
    interpolated_path.emplace_back(base_path.front());
    return interpolated_path;
  }

  std::vector<double> base_path_x(base_path.size());
  std::vector<double> base_path_y(base_path.size());
  std::vector<double> base_path_z(base_path.size());
  std::vector<tf2::Quaternion> base_path_orientation(base_path.size());
  std::vector<double> base_path_s(base_path.size(), 0.0);
  for (size_t i = 0; i < base_path.size(); ++i) {
    base_path_x.at(i) = base_path.at(i).position.x;
    base_path_y.at(i) = base_path.at(i).position.y;
    base_path_z.at(i) = base_path.at(i).position.z;
    tf2::Quaternion src_tf;
    tf2::fromMsg(base_path.at(i).orientation, src_tf);
    base_path_orientation.at(i) = src_tf;
    if (i > 0) {
      base_path_s.at(i) = base_path_s.at(i - 1) + tier4_autoware_utils::calcDistance2d(
                                                    base_path.at(i - 1), base_path.at(i));
    }
  }

  // Prepare resampled s vector
  std::vector<double> resampled_s(frenet_predicted_path.size());
  for (size_t i = 0; i < frenet_predicted_path.size(); ++i) {
    resampled_s.at(i) = frenet_predicted_path.at(i).s;
  }

  // Linear Interpolation for x, y, z, and orientation
  std::vector<double> lerp_ref_path_x = interpolationLerp(base_path_s, base_path_x, resampled_s);
  std::vector<double> lerp_ref_path_y = interpolationLerp(base_path_s, base_path_y, resampled_s);
  std::vector<double> lerp_ref_path_z = interpolationLerp(base_path_s, base_path_z, resampled_s);
  std::vector<tf2::Quaternion> lerp_ref_path_orientation =
    interpolationLerp(base_path_s, base_path_orientation, resampled_s);

  interpolated_path.resize(interpolate_num);
  for (size_t i = 0; i < interpolate_num; ++i) {
    geometry_msgs::msg::Pose interpolated_pose;
    interpolated_pose.position = tier4_autoware_utils::createPoint(
      lerp_ref_path_x.at(i), lerp_ref_path_y.at(i), lerp_ref_path_z.at(i));
    interpolated_pose.orientation = tf2::toMsg(lerp_ref_path_orientation.at(i));
    interpolated_path.at(i) = interpolated_pose;
  }
  return interpolated_path;
}

PredictedPath PathGenerator::convertToPredictedPath(
  const TrackedObject & object, const FrenetPath & frenet_predicted_path,
  const PosePath & ref_path) const
{
  // Object position
  const auto & object_pose = object.kinematics.pose_with_covariance.pose;
  const double object_height = object.shape.dimensions.z / 2.0;

  // Convert Frenet Path to Cartesian Path
  PredictedPath predicted_path;
  predicted_path.time_step = rclcpp::Duration::from_seconds(sampling_time_interval_);
  predicted_path.path.resize(ref_path.size());

  // Set the first point as the object's current position
  predicted_path.path.at(0) = object_pose;

  for (size_t i = 1; i < predicted_path.path.size(); ++i) {
    // Reference Point from interpolated reference path
    const auto & ref_pose = ref_path.at(i);

    // Frenet Point from frenet predicted path
    const auto & frenet_point = frenet_predicted_path.at(i);
    double d_offset = frenet_point.d;

    // Converted Pose
    auto predicted_pose = tier4_autoware_utils::calcOffsetPose(ref_pose, 0.0, d_offset, 0.0);
    predicted_pose.position.z += object_height;

    const double yaw = tier4_autoware_utils::calcAzimuthAngle(
      predicted_path.path.at(i - 1).position, predicted_pose.position);
    predicted_pose.orientation = tier4_autoware_utils::createQuaternionFromYaw(yaw);

    predicted_path.path.at(i) = predicted_pose;
  }

  return predicted_path;
}

FrenetPoint PathGenerator::getFrenetPoint(
  const TrackedObject & object, const geometry_msgs::msg::Pose & ref_pose) const
{
  FrenetPoint frenet_point;

  // 1. Position
  const auto obj_point = object.kinematics.pose_with_covariance.pose.position;
  const float obj_yaw =
    static_cast<float>(tf2::getYaw(object.kinematics.pose_with_covariance.pose.orientation));
  const float lane_yaw = static_cast<float>(tf2::getYaw(ref_pose.orientation));
  frenet_point.s = (obj_point.x - ref_pose.position.x) * cos(lane_yaw) +
                   (obj_point.y - ref_pose.position.y) * sin(lane_yaw);
  frenet_point.d = -(obj_point.x - ref_pose.position.x) * sin(lane_yaw) +
                   (obj_point.y - ref_pose.position.y) * cos(lane_yaw);

  // 2. Velocity
  const float vx = static_cast<float>(object.kinematics.twist_with_covariance.twist.linear.x);
  const float vy = static_cast<float>(object.kinematics.twist_with_covariance.twist.linear.y);
  const float delta_yaw = obj_yaw - lane_yaw;
  frenet_point.s_vel = vx * std::cos(delta_yaw) - vy * std::sin(delta_yaw);
  frenet_point.d_vel = vx * std::sin(delta_yaw) + vy * std::cos(delta_yaw);
  frenet_point.s_acc = 0.0;
  frenet_point.d_acc = 0.0;

  return frenet_point;
}
}  // namespace map_based_prediction
