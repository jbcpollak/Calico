#include "calico/sensors/camera_models.h"

namespace calico::sensors {

std::unique_ptr<CameraModel> CameraModel::Create(
    CameraIntrinsicsModel camera_model) {
  switch (camera_model) {
    case CameraIntrinsicsModel::kOpenCv5: {
      return std::make_unique<OpenCv5Model>();
    }
    case CameraIntrinsicsModel::kOpenCv8: {
      return std::make_unique<OpenCv8Model>();
    }
    case CameraIntrinsicsModel::kKannalaBrandt: {
      return std::make_unique<KannalaBrandtModel>();
    }
    case CameraIntrinsicsModel::kDoubleSphere: {
      return std::make_unique<DoubleSphereModel>();
    }
    case CameraIntrinsicsModel::kFieldOfView: {
      return std::make_unique<FieldOfViewModel>();
    }
    case CameraIntrinsicsModel::kUnifiedCamera: {
      return std::make_unique<UnifiedCameraModel>();
    }
    default: {
      return std::move(std::unique_ptr<CameraModel>{});
    }
  }
}

} // namespace calico::sensors
