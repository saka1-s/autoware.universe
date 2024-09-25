// Copyright 2024 TIER IV, Inc.
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
#ifndef AUTOWARE__BEHAVIOR_PATH_LANE_CHANGE_MODULE__UTILS__CALCULATION_HPP_
#define AUTOWARE__BEHAVIOR_PATH_LANE_CHANGE_MODULE__UTILS__CALCULATION_HPP_

#include "autoware/behavior_path_lane_change_module/utils/data_structs.hpp"

#include <autoware/route_handler/route_handler.hpp>

namespace autoware::behavior_path_planner::utils::lane_change::calculation
{
using autoware::route_handler::Direction;
using autoware::route_handler::RouteHandler;
using behavior_path_planner::lane_change::CommonDataPtr;
using behavior_path_planner::lane_change::LCParamPtr;
using behavior_path_planner::lane_change::PhaseMetrics;

/**
 * @brief Calculates the distance from the ego vehicle to the terminal point.
 *
 * This function computes the distance from the current position of the ego vehicle
 * to either the goal pose or the end of the current lane, depending on whether
 * the vehicle's current lane is within the goal section.
 *
 * @param common_data_ptr Shared pointer to a CommonData structure, which should include:
 *  - Non null `lanes_ptr` that points to the current lanes data.
 *  - Non null `self_odometry_ptr` that contains the current pose of the ego vehicle.
 *  - Non null `route_handler_ptr` that contains the goal pose of the route.
 *
 * @return The distance to the terminal point (either the goal pose or the end of the current lane)
 * in meters.
 */
double calc_ego_dist_to_terminal_end(const CommonDataPtr & common_data_ptr);

double calc_dist_from_pose_to_terminal_end(
  const CommonDataPtr & common_data_ptr, const lanelet::ConstLanelets & lanes,
  const Pose & src_pose);

/**
 * @brief Calculates the minimum stopping distance to terminal start.
 *
 * This function computes the minimum stopping distance to terminal start based on the
 * minimum lane changing velocity and the minimum longitudinal acceleration. It then
 * compares this calculated distance with a pre-defined backward length buffer parameter
 * and returns the larger of the two values to ensure safe lane changing.
 *
 * @param lc_param_ptr Shared pointer to an LCParam structure, which should include:
 *  - `minimum_lane_changing_velocity`: The minimum velocity required for lane changing.
 *  - `min_longitudinal_acc`: The minimum longitudinal acceleration used for deceleration.
 *  - `backward_length_buffer_for_end_of_lane`: A predefined backward buffer length parameter.
 *
 * @return The required backward buffer distance in meters.
 */
double calc_stopping_distance(const LCParamPtr & lc_param_ptr);

/**
 * @brief Calculates the distance to last fit width position along the lane
 *
 * This function computes the distance from the current ego position to the last
 * position along the lane where the ego foot prints stays within the lane
 * boundaries.
 *
 * @param lanelets current ego lanelets
 * @param src_pose source pose to calculate distance from
 * @param bpp_param common parameters used in behavior path planner.
 * @param margin additional margin for checking lane width
 * @return distance to last fit width position along the lane
 */
double calc_dist_to_last_fit_width(
  const lanelet::ConstLanelets lanelets, const Pose & src_pose,
  const BehaviorPathPlannerParameters & bpp_param, const double margin = 0.1);

/**
 * @brief Calculates the maximum preparation longitudinal distance for lane change.
 *
 * This function computes the maximum distance that the ego vehicle can travel during
 * the preparation phase of a lane change. The distance is calculated as the product
 * of the maximum lane change preparation duration and the maximum velocity of the ego vehicle.
 *
 * @param common_data_ptr Shared pointer to a CommonData structure, which should include:
 *  - `lc_param_ptr->lane_change_prepare_duration`: The duration allowed for lane change
 * preparation.
 *  - `bpp_param_ptr->max_vel`: The maximum velocity of the ego vehicle.
 *
 * @return The maximum preparation longitudinal distance in meters.
 */
double calc_maximum_prepare_length(const CommonDataPtr & common_data_ptr);

/**
 * @brief Calculates the distance from the ego vehicle to the start of the target lanes.
 *
 * This function computes the shortest distance from the current position of the ego vehicle
 * to the start of the target lanes by measuring the arc length to the front points of
 * the left and right boundaries of the target lane. If the target lanes are empty or other
 * required data is unavailable, the function returns numeric_limits<double>::max() preventing lane
 * change being executed.
 *
 * @param common_data_ptr Shared pointer to a CommonData structure, which should include:
 *  - `route_handler_ptr`: Pointer to the route handler that manages the route.
 *  - Other necessary parameters for ego vehicle positioning.
 * @param current_lanes The set of lanelets representing the current lanes of the ego vehicle.
 * @param target_lanes The set of lanelets representing the target lanes for lane changing.
 *
 * @return The distance from the ego vehicle to the start of the target lanes in meters,
 * or numeric_limits<double>::max() if the target lanes are empty or data is unavailable.
 */
double calc_ego_dist_to_lanes_start(
  const CommonDataPtr & common_data_ptr, const lanelet::ConstLanelets & current_lanes,
  const lanelet::ConstLanelets & target_lanes);

double calc_minimum_lane_change_length(
  const LaneChangeParameters & lane_change_parameters, const std::vector<double> & shift_intervals);

double calc_minimum_lane_change_length(
  const std::shared_ptr<RouteHandler> & route_handler, const lanelet::ConstLanelet & lane,
  const LaneChangeParameters & lane_change_parameters, Direction direction);

double calc_maximum_lane_change_length(
  const double current_velocity, const LaneChangeParameters & lane_change_parameters,
  const std::vector<double> & shift_intervals, const double max_acc);

double calc_maximum_lane_change_length(
  const CommonDataPtr & common_data_ptr, const lanelet::ConstLanelet & current_terminal_lanelet,
  const double max_acc);

/**
 * @brief Calculates the distance required during a lane change operation.
 *
 * Used for computing prepare or lane change length based on current and maximum velocity,
 * acceleration, and duration, returning the lesser of accelerated distance or distance at max
 * velocity.
 *
 * @param velocity The current velocity of the vehicle in meters per second (m/s).
 * @param maximum_velocity The maximum velocity the vehicle can reach in meters per second (m/s).
 * @param acceleration The acceleration of the vehicle in meters per second squared (m/s^2).
 * @param duration The duration of the lane change in seconds (s).
 * @return The calculated minimum distance in meters (m).
 */
double calc_phase_length(
  const double velocity, const double maximum_velocity, const double acceleration,
  const double duration);

double calc_lane_changing_acceleration(
  const double initial_lane_changing_velocity, const double max_path_velocity,
  const double lane_changing_time, const double prepare_longitudinal_acc);

std::vector<PhaseMetrics> calc_prepare_phase_metrics(
  const CommonDataPtr & common_data_ptr, const std::vector<double> & prepare_durations,
  const std::vector<double> & lon_accel_values, const double current_velocity,
  const double min_length_threshold = 0.0,
  const double max_length_threshold = std::numeric_limits<double>::max());

std::vector<PhaseMetrics> calc_shift_phase_metrics(
  const CommonDataPtr & common_data_ptr, const double shift_length, const double initial_velocity,
  const double max_velocity, const double lon_accel,
  const double max_length_threshold = std::numeric_limits<double>::max());
}  // namespace autoware::behavior_path_planner::utils::lane_change::calculation

#endif  // AUTOWARE__BEHAVIOR_PATH_LANE_CHANGE_MODULE__UTILS__CALCULATION_HPP_
