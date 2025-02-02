#pragma once

#include "mab_types.hpp"
#include "kalman_filter.hpp"

#include <cmath>
#include <cstdint>
#include <chrono>
#include <vector>
#include <map>
#include <algorithm>
#include <numeric>
#include <Eigen/Dense>

typedef std::map<std::string, double> MotorStatus_T;
typedef std::map<std::string, float> MotorCommand_T;
typedef std::vector<float>  FilterVector;

namespace mab
{
/**
 * @class Md80
 * @brief Contains settings and state of a single md80 drive.
 * Can be used to change control parameters and read the state of the drive.
 */
class Md80
{
   private:
	uint16_t canId;

	// the current status of the motor
	float position = 0.0f;
	float velocity = 0.0f;
	float torque = 0.0f;
	float prevPosition = 0.0f;
    double prevTime = 0.0;
    MotorStatus_T motorStatus;
	uint8_t temperature = 0;
	uint16_t errorVector = 0;

	// transmit values
	Md80Mode_E controlMode = Md80Mode_E::IDLE;
	float positionTarget = 0.0f;
	float velocityTarget = 0.0f;
	float torqueSet = 0.0f;
	float maxTorque = 1.0f;
	float maxVelocity = 100.0f;
	int frameId = -1;
	RegPid_t velocityController;
	RegPid_t positionController;
	RegImpedance_t impedanceController;

	bool regulatorsAdjusted = false;
	bool velocityRegulatorAdjusted = false;
	StdMd80CommandFrame_t commandFrame;
	StdMd80ResponseFrame_t responseFrame;

	// watchdog paramters
    float maxMotorPosition = 6.2831;
    float minMotorPosition = -6.2831;
    float softLimitFactor = 1.0;
    float softMaxPosition = 6.2831;
    float softMinPosition = -6.2831;
    float watchdogKP = 10.0f;
    float watchdogKD = 0.0f;
    float watchdogTorqueOffset = 0.0f;
    float watchdogPosPercentage = 1.0;
        
    // requested controll
    float requestedPosition = 0.0f;
    float requestedVelocity = 0.0f;
    float requestorqueSet = 0.0f;
    RegImpedance_t requestedImpedanceController;
    bool requestKpKdAdjusted = false;
    bool printWatchdog = true;

	// high_pid_params
    bool useHighPid = false;
    float h_kp = 0.0f;
    float h_kd = 0.0f;
    float h_ki = 0.0f;
    float h_maxAggError = 0.0f;
    float h_limit_scale = 0.0f;
    std::vector<float> h_aggError;
    float pidPos = 0.0f;
    int aggErrCurrIndex = 0;

    // Savgol filter
    bool do_savgol = false;
    FilterVector savgolCoeffs;
    int savgolSizeOfBuffer = 0;
    FilterVector savgolPosBuffer;
    float savgolVelocity = 0.0f;

    // Kalman filter vars
    bool do_kalman_filter = false;
    int number_of_states = 2;
    int number_of_mesurments = 2;
    double dt = 1.0/500;
    Eigen::MatrixXd A; // System dynamics matrix
    Eigen::MatrixXd C; // Output matrix
    Eigen::MatrixXd Q; // Process noise covariance
    Eigen::MatrixXd R; // Measurement noise covariance
    Eigen::MatrixXd P; // Estimate error covariance
    KalmanFilter kf;

	// Private functions
	void packImpedanceFrame();
	void packPositionFrame();
	void packVelocityFrame();
	void packMotionTargetsFrame();

	// our private add function
    void watchdog();
    void updateTargets();
    void setImpedanceControllerParams(float kp, float kd);
    void pid();
    float savgol(double newPos);
    float kalman_filter();

   public:
	/**
	 * @brief Construct a new Md80 object
	 *
	 * @param canID FDACN Id of the drive
	 */
	Md80(uint16_t canID, MotorCommand_T config);
	Md80(uint16_t canID);
	/**
	 * @brief Destroy the Md80 objec
	 */
	~Md80();
	/**
	 * @brief Set the Position PID Regulator parameters.
	 * @note Regulator output is target velocity in rad/s. The output is then passed as input to Velocity PID regulator.
	 * @param kp proportional gain
	 * @param ki integral gain
	 * @param kd derivative gain
	 * @param iWindup anti-windup - maximal output of the integral (i) part of the regulator
	 */
	void setPositionControllerParams(float kp, float ki, float kd, float iWindup);
	/**
	 * @brief Set the Velocity PID Regulator parameters.
	 * @note Regulator output is Torque in Nm. The output is then passed directly to internal current/torque regulator.
	 * @param kp proportional gain
	 * @param ki integral gain
	 * @param kd derivative gain
	 * @param iWindup anti-windup - maximal output of the integral (i) part of the regulator
	 */
	void setVelocityControllerParams(float kp, float ki, float kd, float iWindup);
	/**
	 * @brief Set the Impedance Regulator parameters.
	 * @param kp Displacement gain ( analogic to 'k' parameter of the spring-damper equation)
	 * @param kd Damping coefficient (analogin to 'b' parameter of the spring-damper equation)
	 */
	void setImpedanceRequestedControllerParams(float kp, float kd);
	void setPIDParams(MotorCommand_T pidParams);
	/**
     * @brief Set the coeffs for savgol filter vel compute
     * @param coeef a vector of float coeffeciants for savgol filter
     */
    void setSavgolCoeffs(FilterVector coeffs);
    void setKalmanFilter(FilterVector processNoiseCov, FilterVector measurmentNoiseCov, FilterVector initailStateError, int frequency);

	// simple setters
	/**
	 * @brief Set the Max Torque object
	 * @note This can be overriden by current limiter set by 'Candle.configMd80CurrentLimit'. Current/torque
	 * will be limited to whichever limit has a lower value.
	 * @note This is only applied with CUSTOM Impedance/Velocity PID controller settings.
	 * @param maxTorque Torque limit for PID/Impedance regulators
	 */
	void setMaxTorque(float maxTorque);
	/**
	 * @brief Set the Max Velocity for Position PID and Velocity PID modes.
	 * @note This is only applied with CUSTOM Position PID and Velocity PID controller settings.
	 * @note Has no effect in Torque or Impedance mode.
	 * @param maxVelocity
	 */
	void setMaxVelocity(float maxVelocity);
	/**
	 * @brief Set the Target Position for Position PID and Impedance modes.
	 * @param target target position in radians
	 */
	void setTargetPosition(float target);
	/**
	 * @brief Set the Target Velocity for Velocity PID and Impedance modes.
	 * @param target target velocity in rad/s (radians per second)
	 */
	void setTargetVelocity(float target) { velocityTarget = target; };
	/**
	 * @brief Set the Torque Command for TORQUE and Impedance (torque_ff) modes.
	 * @param target target torque in Nm (Newton-meters)
	 */
	void setTorque(float target) { torqueSet = target; };

	/**
     * @brief Set the frame id of the transmit
     * @param target target position in radians
     */
    void setFrameId(int frameId) { this->frameId = frameId; };
    /**
     * @brief get the frame id
     */
    int getFrameId() {return frameId;};

	// getters
	/**
	 * @brief Get the Error Vector of the md80
	 * @return uint16_t vector with per-bit coded errors. Refer to documentation for meaning of error codes.
	 */
	uint16_t getErrorVector() { return errorVector; };

	/**
	 * @brief Get the FDCAN Id of the drive
	 * @return uint16_t FDCAN Id (10 - 2047)
	 */
	uint16_t getId() { return canId; };
	/**
	 * @brief Get the Position of md80
	 * @return float angular position in radians
	 */
	float getPosition() { return position; };
	/**
	 * @brief Get the Velocity of md80
	 * @return float angular velocity in rad/s (radians per second)
	 */
	float getVelocity() { return velocity; };
	/**
	 * @brief Get the Torque of md80
	 * @return float torque in Nm (Newton-meters)
	 */
	float getTorque() { return torque; };
	/**
     * @brief Get the kp of md80
     * @return float angular velocity in rad/s (radians per second)
     */
    float getKP() { return impedanceController.kp; };
    /**
     * @brief Get the kd of md80
     * @return float angular velocity in rad/s (radians per second)
     */
    float getKD() { return impedanceController.kd; };
    /**
     * @brief Get the target position of md80
     * @return float angular velocity in rad/s (radians per second)
     */
    float getTargetPos() { return positionTarget; };
    /**
     * @brief Get the target velocity of md80
     * @return float angular velocity in rad/s (radians per second)
     */
    float getTargetVel() { return velocityTarget; };
    /**
     * @brief Get the torque request of md80
     * @return float angular velocity in rad/s (radians per second)
     */
    float getTorqueRequest() { return torqueSet; };

	MotorStatus_T getMotorStatus() { return motorStatus; };
	/**
	 * @brief Get the exteral thermistor temperature reading
	 * @return uint8_t internally computed temperature value in *C 
	 */
	uint8_t getTemperature() { return temperature; };

	/**
	 * @brief For internal use by CANdle only.
	 * @private
	 */
	StdMd80CommandFrame_t __getCommandFrame() { return commandFrame; };
	/**
	 * @brief For internal use by CANdle only. Updates FDCAN frame based on parameters.
	 * @private
	 */
	void __updateCommandFrame();
	/**
	 * @brief For internal use by CANdle only. Updates FDCAN frame parameters.
	 * @private
	 */
	void __updateResponseData(StdMd80ResponseFrame_t* _responseFrame);
	/**
     * @brief For internal use by CANdle only. Updates FDCAN frame parameters with candle seq number
     * @private
     */
    void __updateResponseData(StdMd80ResponseFrame_t *_responseFrame, double time, int seq);
	/**
	 * @brief For internal use by CANdle only. Updates regulatorsAdjusted.
	 * @private
	 */
	void __updateRegulatorsAdjusted(bool adjusted);
	/**
	 * @brief For internal use by CANdle only.
	 * @private
	 */
	void __setControlMode(Md80Mode_E mode);
};
}  // namespace mab