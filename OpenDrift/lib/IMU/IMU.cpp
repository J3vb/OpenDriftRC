#include "IMU.h"

#include <math.h>


bool IMU::begin()
{
    Wire.begin(SDA_PIN, SCL_PIN);


    if (!qmi.begin(
        Wire,
        QMI8658_L_SLAVE_ADDRESS,
        SDA_PIN,
        SCL_PIN))
    {
        return false;
    }


    // A sensor that answers WHO_AM_I but refuses a configuration write is
    // not usable: SensorLib leaves the gyro disabled and every later read
    // fails without bus traffic. Report it so setup() can retry or reboot.
    bool configured =
        qmi.configAccelerometer(
            SensorQMI8658::ACC_RANGE_4G,
            SensorQMI8658::ACC_ODR_1000Hz,
            SensorQMI8658::LPF_MODE_0
        );

    configured =
        qmi.configGyroscope(
            SensorQMI8658::GYR_RANGE_1024DPS,
            SensorQMI8658::GYR_ODR_896_8Hz,
            SensorQMI8658::LPF_MODE_0
        ) && configured;

    gyroLpfMode = 0;
    lpfRetryMode = 0;
    lpfRetryCount = 0;
    lpfLastAttemptMs = 0;
    enableRetryMs = 0;
    settleUntilMs = 0;

    configured = qmi.enableAccelerometer() && configured;
    configured = qmi.enableGyroscope() && configured;

    return configured;
}


bool IMU::setGyroLpfMode(uint8_t mode)
{
    mode = constrain(mode, 0, 2);

    // A gyro left disabled by an earlier failed write is re-enabled here,
    // before any early return, so no path can leave it off until reboot.
    // The 1 s spacing keeps a dead bus from being hammered every tick.
    if(
        !qmi.isEnableGyroscope() &&
        millis() - enableRetryMs >= 1000
    )
    {
        enableRetryMs = millis();

        if(qmi.enableGyroscope())
        {
            settleUntilMs = millis() + GYRO_SETTLE_MS;
        }
    }

    if(mode == gyroLpfMode)
    {
        return true;
    }

    // A sensor that refuses the write must not be hammered once per control
    // tick. Back off for a second, then give up on that mode entirely.
    if(mode != lpfRetryMode)
    {
        lpfRetryMode = mode;
        lpfRetryCount = 0;
        lpfLastAttemptMs = 0;
    }

    if(lpfRetryCount >= 5)
    {
        return false;
    }

    if(
        lpfRetryCount > 0 &&
        millis() - lpfLastAttemptMs < 1000
    )
    {
        return false;
    }

    SensorQMI8658::LpfMode sensorMode =
        mode == 1
        ? SensorQMI8658::LPF_MODE_3
        : (mode == 2
            ? SensorQMI8658::LPF_OFF
            : SensorQMI8658::LPF_MODE_0);

    bool configured =
        qmi.configGyroscope(
            SensorQMI8658::GYR_RANGE_1024DPS,
            SensorQMI8658::GYR_ODR_896_8Hz,
            sensorMode
        );

    // configGyroscope() disables the gyro first and only re-enables it
    // after the last register write. A failed write, or a retry that
    // starts with the gyro already off, would otherwise leave it disabled
    // and every read failing until reboot.
    if(!qmi.isEnableGyroscope())
    {
        configured = qmi.enableGyroscope() && configured;
    }

    if(!configured)
    {
        lpfLastAttemptMs = millis();

        if(lpfRetryCount < 255)
        {
            lpfRetryCount++;
        }

        return false;
    }

    gyroLpfMode = mode;
    lpfRetryCount = 0;
    lpfLastAttemptMs = 0;

    // The gyro was stopped and restarted. Its first samples are not valid
    // yet, so yaw reads as invalid until the turn-on time has passed.
    settleUntilMs = millis() + GYRO_SETTLE_MS;

    return true;
}


uint8_t IMU::getGyroLpfMode() const
{
    return gyroLpfMode;
}



void IMU::update()
{
    uint32_t now = micros();

    float dt = 0.01f;

    if(lastUpdateMicros != 0)
    {
        dt =
            (now - lastUpdateMicros)
            /
            1000000.0f;

        dt = constrain(
            dt,
            0.001f,
            0.05f
        );
    }

    lastUpdateMicros = now;

    if(!qmi.getGyroscope(
        gyroX,
        gyroY,
        gyroZ
    ))
    {
        if(gyroReadFailures < 255)
        {
            gyroReadFailures++;
        }

        gyroReadOk = false;

        return;
    }

    gyroReadFailures = 0;
    gyroReadOk = true;

    if(!qmi.getAccelerometer(
        accelX,
        accelY,
        accelZ
    ))
    {
        if(accelReadFailures < 255)
        {
            accelReadFailures++;
        }

        return;
    }

    accelReadFailures = 0;

    accelMagnitude = sqrtf(
        (accelX * accelX) +
        (accelY * accelY) +
        (accelZ * accelZ)
    );

    tiltRate = sqrtf(
        (gyroX * gyroX) +
        (gyroY * gyroY)
    );

    if(!accelFilterReady)
    {
        slowAccelX = accelX;
        slowAccelY = accelY;
        slowAccelZ = accelZ;
        accelFilterReady = true;
    }

    float slowAmount =
        1.0f - expf(-dt / 0.25f);

    slowAccelX +=
        (accelX - slowAccelX) * slowAmount;
    slowAccelY +=
        (accelY - slowAccelY) * slowAmount;
    slowAccelZ +=
        (accelZ - slowAccelZ) * slowAmount;

    float deltaX = accelX - slowAccelX;
    float deltaY = accelY - slowAccelY;
    float deltaZ = accelZ - slowAccelZ;

    accelDelta = sqrtf(
        (deltaX * deltaX) +
        (deltaY * deltaY) +
        (deltaZ * deltaZ)
    );

    // Terrain detector used to release settled-drift features during a hard
    // compression, unload, or pitch/roll impulse. It never creates steering
    // correction directly.
    float accelerationScore = constrain(
        (accelDelta - 0.06f) / 0.50f,
        0.0f,
        1.0f
    );

    float tiltScore = constrain(
        (tiltRate - 15.0f) / 180.0f,
        0.0f,
        1.0f
    );

    float unloadScore = constrain(
        (0.75f - accelMagnitude) / 0.55f,
        0.0f,
        1.0f
    );

    float scoreTarget = max(
        accelerationScore,
        max(
            tiltScore * 0.80f,
            unloadScore
        )
    );

    float scoreTimeConstant =
        scoreTarget > surfaceDisturbanceScore
        ?
        0.04f
        :
        0.20f;

    float scoreAmount =
        1.0f - expf(-dt / scoreTimeConstant);

    surfaceDisturbanceScore +=
        (scoreTarget - surfaceDisturbanceScore)
        *
        scoreAmount;

    surfaceDisturbanceScore = constrain(
        surfaceDisturbanceScore,
        0.0f,
        1.0f
    );
}



bool IMU::isSettling() const
{
    return
        settleUntilMs != 0 &&
        (int32_t)(millis() - settleUntilMs) < 0;
}



bool IMU::isYawValid() const
{
    return gyroReadFailures < 3 && !isSettling();
}



bool IMU::lastGyroReadOk() const
{
    return gyroReadOk && !isSettling();
}



bool IMU::isHealthy() const
{
    return gyroReadFailures < 25;
}



bool IMU::isAccelHealthy() const
{
    return accelReadFailures < 25;
}



float IMU::getGyroX()
{
    return gyroX;
}



float IMU::getGyroY()
{
    return gyroY;
}



float IMU::getYawRate()
{
    return gyroZ;
}


float IMU::getAccelX()
{
    return accelX;
}


float IMU::getAccelY()
{
    return accelY;
}


float IMU::getAccelZ()
{
    return accelZ;
}


float IMU::getAccelMagnitude()
{
    return accelMagnitude;
}


float IMU::getAccelDelta()
{
    return accelDelta;
}


float IMU::getTiltRate()
{
    return tiltRate;
}


float IMU::getSurfaceDisturbanceScore()
{
    return surfaceDisturbanceScore;
}
