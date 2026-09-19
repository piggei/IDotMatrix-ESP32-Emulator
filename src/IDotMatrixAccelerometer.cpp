#include "IDotMatrixAccelerometer.h"

#if IDOTMATRIX_ORIENTATION_SENSOR

#if defined(IDOTMATRIX_ACCEL_DRIVER_LIS3DH)
  #include "drivers/IDotMatrixAccelLIS3DH.h"
#elif defined(IDOTMATRIX_ACCEL_DRIVER_MPU6050)
  #error "IDOTMATRIX_ACCEL_DRIVER_MPU6050 is reserved for the future GY-521 backend but is not implemented yet"
#else
  #error "IDOTMATRIX_ORIENTATION_SENSOR enabled without a supported accelerometer backend"
#endif

bool idotAccelBegin() {
#if defined(IDOTMATRIX_ACCEL_DRIVER_LIS3DH)
  return idotAccelLIS3DHBegin();
#endif
}

bool idotAccelRead(IDotMatrixAccelSample &sample) {
#if defined(IDOTMATRIX_ACCEL_DRIVER_LIS3DH)
  return idotAccelLIS3DHRead(sample);
#endif
}

const char *idotAccelDriverName() {
#if defined(IDOTMATRIX_ACCEL_DRIVER_LIS3DH)
  return "LIS3DH";
#else
  return "unknown";
#endif
}

#endif
