#pragma once
/*
 * MikroDuino — Master Include Header
 *
 * Pulls in every core hardware peripheral. There are no opt-in utility
 * subsystems in this build — just include this header.
 */

// ---- Core hardware (always present) ----
#include "platform.hpp"
#include "registers.hpp"
#include "gpio.hpp"
#include "usart.hpp"
#include "spi.hpp"
#include "i2c.hpp"
#include "adc.hpp"
#include "timer.hpp"
#include "pwm.hpp"
#include "interrupt.hpp"
#include "eeprom.hpp"
