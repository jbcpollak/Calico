#include "calico/batch_optimizer.h"

#include "calico/matchers.h"
#include "calico/test_utils.h"
#include "calico/sensors/camera.h"
#include "calico/sensors/gyroscope.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace calico {
namespace {

class BatchOptimizerTest : public ::testing::Test {
 protected:
  absl::flat_hash_map<double, Pose3d> poses_world_sensorrig;
  std::vector<Eigen::Vector3d> t_world_points;
  std::vector<double> stamps;

  void SetUp() override {
    DefaultSyntheticTest testing_fixture;
    poses_world_sensorrig = testing_fixture.TrajectoryAsMap();
    t_world_points = testing_fixture.WorldPoints();
    stamps = testing_fixture.TrajectoryMapKeys();
  }
};

TEST_F(BatchOptimizerTest, ToyStereoCameraAndImuCalibration) {
  // Construct a world model consisting of a single planar object.
  RigidBody planar_target{
    .world_pose_is_constant = true,
    .model_definition_is_constant = true,
  };
  for (int i = 0; i < t_world_points.size(); ++i) {
    planar_target.model_definition[i] = t_world_points[i];
  }
  WorldModel* world_model = new WorldModel;
  EXPECT_OK(world_model->AddRigidBody(planar_target));
  // Construct the sensorrig trajectory.
  Trajectory* trajectory_world_sensorrig = new Trajectory;
  ASSERT_OK(trajectory_world_sensorrig->AddPoses(poses_world_sensorrig));

  // Construct ground truth cameras and measurements.
  const sensors::CameraIntrinsicsModel kCameraModel =
      sensors::CameraIntrinsicsModel::kOpenCv5;
  constexpr double kStereoRotationAngle = 2.0 * M_PI / 180.0;
  constexpr double kStereoBaseline = 0.05;
  constexpr double kRightCameraLatency = 0.01;
  Eigen::VectorXd true_camera_intrinsics(sensors::OpenCv5Model::kNumberOfParameters);
  true_camera_intrinsics <<
    785, 640, 400, -3.149e-1, 1.069e-1, 1.616e-4, 1.141e-4, -1.853e-2;
  Pose3d true_extrinsics_left;
  Pose3d true_extrinsics_right;
  true_extrinsics_right.rotation() =
      Eigen::Quaterniond(
          Eigen::AngleAxisd(
              kStereoRotationAngle, Eigen::Vector3d::Random().normalized()));
  true_extrinsics_right.translation() =
      kStereoBaseline * Eigen::Vector3d::Random();
  sensors::Camera true_camera_left;
  EXPECT_OK(true_camera_left.SetModel(kCameraModel));
  EXPECT_OK(true_camera_left.SetIntrinsics(true_camera_intrinsics));
  true_camera_left.SetExtrinsics(true_extrinsics_left);
  sensors::Camera true_camera_right;
  EXPECT_OK(true_camera_right.SetModel(kCameraModel));
  EXPECT_OK(true_camera_right.SetIntrinsics(true_camera_intrinsics));
  true_camera_right.SetExtrinsics(true_extrinsics_right);
  EXPECT_OK(true_camera_right.SetLatency(kRightCameraLatency));
  std::vector<sensors::CameraMeasurement> measurements_left, measurements_right;
  ASSERT_OK_AND_ASSIGN(measurements_left,
      true_camera_left.Project(stamps, *trajectory_world_sensorrig,
                               *world_model));
  ASSERT_OK_AND_ASSIGN(measurements_right,
      true_camera_right.Project(stamps, *trajectory_world_sensorrig,
                                *world_model));
  // Construct ground truth gyroscope and measurements.
  const sensors::GyroscopeIntrinsicsModel kGyroscopeModel =
      sensors::GyroscopeIntrinsicsModel::kScaleAndBias;
  constexpr double kGyroscopeRotationAngle = 2.0 * M_PI / 180.0;
  constexpr double kGyroscopeLatency = 0.02;
  Eigen::VectorXd true_gyroscope_intrinsics(
      sensors::ScaleAndBiasModel::kNumberOfParameters);
  true_gyroscope_intrinsics << 1.3, 0.01, -0.01, 0.01;
  Pose3d true_extrinsics_gyroscope;
  true_extrinsics_gyroscope.rotation() =
      Eigen::Quaterniond(
          Eigen::AngleAxisd(
              kGyroscopeRotationAngle, Eigen::Vector3d::Random().normalized()));
  sensors::Gyroscope true_gyroscope;
  EXPECT_OK(true_gyroscope.SetModel(kGyroscopeModel));
  EXPECT_OK(true_gyroscope.SetIntrinsics(true_gyroscope_intrinsics));
  true_gyroscope.SetExtrinsics(true_extrinsics_gyroscope);
  EXPECT_OK(true_gyroscope.SetLatency(kGyroscopeLatency));
  std::vector<sensors::GyroscopeMeasurement> measurements_gyroscope;
  ASSERT_OK_AND_ASSIGN(measurements_gyroscope,
      true_gyroscope.Project(stamps, *trajectory_world_sensorrig));

  // Create optimization sensors.
  Eigen::VectorXd initial_camera_intrinsics = 1.01 * true_camera_intrinsics;
  initial_camera_intrinsics.tail(5).setZero();
  Pose3d initial_extrinsics_right = true_extrinsics_right;
  initial_extrinsics_right.translation() += 0.01 * Eigen::Vector3d::Random();
  sensors::Camera* camera_left = new sensors::Camera();
  camera_left->SetName("Left");
  EXPECT_OK(camera_left->SetModel(kCameraModel));
  EXPECT_OK(camera_left->SetIntrinsics(initial_camera_intrinsics));
  camera_left->EnableExtrinsicsEstimation(false);
  camera_left->EnableIntrinsicsEstimation(true);
  camera_left->EnableLatencyEstimation(false);
  EXPECT_OK(camera_left->AddMeasurements(measurements_left));
  sensors::Camera* camera_right = new sensors::Camera();
  camera_right->SetName("Right");
  EXPECT_OK(camera_right->SetModel(kCameraModel));
  EXPECT_OK(camera_right->SetIntrinsics(initial_camera_intrinsics));
  camera_right->SetExtrinsics(initial_extrinsics_right);
  camera_right->EnableExtrinsicsEstimation(true);
  camera_right->EnableIntrinsicsEstimation(true);
  camera_right->EnableLatencyEstimation(true);
  EXPECT_OK(camera_right->AddMeasurements(measurements_right));
  Eigen::VectorXd initial_gyroscope_intrinsics =
      1.01 * true_gyroscope_intrinsics;
  Pose3d initial_gyroscope_extrinsics = true_extrinsics_gyroscope;
  sensors::Gyroscope* gyroscope = new sensors::Gyroscope();
  gyroscope->SetName("Gyroscope");
  EXPECT_OK(gyroscope->SetModel(kGyroscopeModel));
  EXPECT_OK(gyroscope->SetIntrinsics(initial_gyroscope_intrinsics));
  gyroscope->SetExtrinsics(initial_gyroscope_extrinsics);
  gyroscope->EnableExtrinsicsEstimation(true);
  gyroscope->EnableIntrinsicsEstimation(true);
  gyroscope->EnableLatencyEstimation(true);
  EXPECT_OK(gyroscope->AddMeasurements(measurements_gyroscope));

  // Construct optimization problem and optimize.
  BatchOptimizer optimizer;
  optimizer.AddSensor(camera_left);
  optimizer.AddSensor(camera_right);
  optimizer.AddSensor(gyroscope);
  optimizer.AddWorldModel(world_model);
  optimizer.AddTrajectory(trajectory_world_sensorrig);
  ASSERT_OK_AND_ASSIGN(auto summary, optimizer.Optimize());

  // Expect near perfect calibration results due to perfect data.
  constexpr double kSmallNumber = 1e-8;
  EXPECT_EQ(summary.termination_type, ceres::CONVERGENCE);
  EXPECT_LT(summary.final_cost, kSmallNumber);
  EXPECT_THAT(true_camera_intrinsics,
              EigenIsApprox(camera_left->GetIntrinsics(), kSmallNumber));
  EXPECT_THAT(true_camera_intrinsics,
              EigenIsApprox(camera_right->GetIntrinsics(), kSmallNumber));
  EXPECT_THAT(true_extrinsics_right, PoseIsApprox(camera_right->GetExtrinsics(),
                                                  kSmallNumber));
  EXPECT_NEAR(kRightCameraLatency, camera_right->GetLatency(), kSmallNumber);
  EXPECT_THAT(true_gyroscope_intrinsics,
              EigenIsApprox(gyroscope->GetIntrinsics(), kSmallNumber));
  EXPECT_THAT(true_extrinsics_gyroscope,
              PoseIsApprox(gyroscope->GetExtrinsics(), kSmallNumber));
  EXPECT_NEAR(kGyroscopeLatency, gyroscope->GetLatency(), kSmallNumber);
  std::cout << summary.FullReport() << std::endl;
}

} // namespace
} // namespace calico
