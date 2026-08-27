#include "myRc522.h"
#include "spi.h"
#include <string.h>

/*
 * MFRC522 통신 구조
 *
 *   STM32 --(SPI2: SCK/MOSI/MISO/CS)--> MFRC522
 *   MFRC522 --(13.56 MHz RF)-----------> RFID 카드/키태그
 *
 * STM32가 RFID 카드와 직접 통신하는 것이 아니다. STM32는 SPI로 MFRC522의
 * 레지스터와 FIFO를 제어하고, 실제 ISO/IEC 14443-A 무선 통신은 MFRC522가
 * 처리한다.
 */

/* MFRC522 commands */
#define RC522_CMD_IDLE          0x00U
#define RC522_CMD_CALC_CRC      0x03U
#define RC522_CMD_TRANSCEIVE    0x0CU
#define RC522_CMD_SOFT_RESET    0x0FU

/* MFRC522 registers */
#define RC522_REG_COMMAND       0x01U
#define RC522_REG_COM_IRQ       0x04U
#define RC522_REG_DIV_IRQ       0x05U
#define RC522_REG_ERROR         0x06U
#define RC522_REG_FIFO_DATA     0x09U
#define RC522_REG_FIFO_LEVEL    0x0AU
#define RC522_REG_CONTROL       0x0CU
#define RC522_REG_BIT_FRAMING   0x0DU
#define RC522_REG_COLL          0x0EU
#define RC522_REG_MODE          0x11U
#define RC522_REG_TX_MODE       0x12U
#define RC522_REG_RX_MODE       0x13U
#define RC522_REG_TX_CONTROL    0x14U
#define RC522_REG_TX_ASK        0x15U
#define RC522_REG_CRC_RESULT_H  0x21U
#define RC522_REG_CRC_RESULT_L  0x22U
#define RC522_REG_MOD_WIDTH     0x24U
#define RC522_REG_T_MODE        0x2AU
#define RC522_REG_T_PRESCALER   0x2BU
#define RC522_REG_T_RELOAD_H    0x2CU
#define RC522_REG_T_RELOAD_L    0x2DU
#define RC522_REG_VERSION       0x37U

/* ISO/IEC 14443-A commands */
#define PICC_CMD_REQA            0x26U
#define PICC_CMD_HALT_A          0x50U
#define PICC_CMD_SEL_CL1         0x93U
#define PICC_CMD_SEL_CL2         0x95U
#define PICC_CMD_SEL_CL3         0x97U
#define PICC_CMD_CT              0x88U
#define PICC_SAK_CASCADE_BIT     0x04U

#define RC522_FIFO_SIZE          64U
#define RC522_SPI_TIMEOUT_MS     10U
#define RC522_TRANSCEIVE_MS      50U
#define RC522_CRC_TIMEOUT_MS     50U

static void rc522Select(void)
{
    /* SPI 버스에서 MFRC522 선택: CS는 Active Low이다. */
    HAL_GPIO_WritePin(RC522CS_GPIO_Port, RC522CS_Pin, GPIO_PIN_RESET);
}

static void rc522Deselect(void)
{
    /* SPI 통신 종료 후 CS를 High로 돌려 다른 장치가 버스를 쓸 수 있게 한다. */
    HAL_GPIO_WritePin(RC522CS_GPIO_Port, RC522CS_Pin, GPIO_PIN_SET);
}

static void rc522WriteRegister(uint8_t reg, uint8_t value)
{
    /*
     * MFRC522 SPI 쓰기 주소 형식:
     *   bit7 = 0(Write), bit6~1 = 레지스터 주소, bit0 = 0
     * 따라서 (reg << 1) & 0x7E로 주소 바이트를 만든다.
     */
    uint8_t data[2] = {
        (uint8_t)((reg << 1U) & 0x7EU),
        value
    };

    rc522Select();
    (void)HAL_SPI_Transmit(&hspi2, data, sizeof(data), RC522_SPI_TIMEOUT_MS);
    rc522Deselect();
}

static uint8_t rc522ReadRegister(uint8_t reg)
{
    /*
     * 읽기는 주소 바이트의 bit7을 1로 만든다. 첫 바이트로 주소를 보내고,
     * 두 번째 더미 바이트를 보내는 동안 MISO로 레지스터 값을 받는다.
     * SPI는 송신해야 동시에 수신할 수 있는 Full-Duplex 통신이다.
     */
    uint8_t tx[2] = {
        (uint8_t)(((reg << 1U) & 0x7EU) | 0x80U),
        0x00U
    };
    uint8_t rx[2] = {0};

    rc522Select();
    (void)HAL_SPI_TransmitReceive(&hspi2, tx, rx, sizeof(tx),
                                  RC522_SPI_TIMEOUT_MS);
    rc522Deselect();

    return rx[1];
}

static void rc522SetBitMask(uint8_t reg, uint8_t mask)
{
    /* 다른 비트는 유지하고 원하는 비트만 1로 만든다. */
    rc522WriteRegister(reg, (uint8_t)(rc522ReadRegister(reg) | mask));
}

static void rc522ClearBitMask(uint8_t reg, uint8_t mask)
{
    /* 다른 비트는 유지하고 원하는 비트만 0으로 만든다. */
    rc522WriteRegister(reg, (uint8_t)(rc522ReadRegister(reg) & ~mask));
}

static bool rc522CalculateCrc(const uint8_t *data, uint8_t length,
                              uint8_t result[2])
{
    if (data == NULL || result == NULL)
        return false;

    /* FIFO에 데이터를 넣고 MFRC522 하드웨어 CRC 계산기를 실행한다. */
    rc522WriteRegister(RC522_REG_COMMAND, RC522_CMD_IDLE);
    rc522WriteRegister(RC522_REG_DIV_IRQ, 0x04U);
    rc522WriteRegister(RC522_REG_FIFO_LEVEL, 0x80U);

    for (uint8_t i = 0; i < length; i++)
        rc522WriteRegister(RC522_REG_FIFO_DATA, data[i]);

    rc522WriteRegister(RC522_REG_COMMAND, RC522_CMD_CALC_CRC);

    /* DivIrqReg의 CRC 완료 비트를 기다리되 무한 대기는 하지 않는다. */
    uint32_t start = HAL_GetTick();
    while ((rc522ReadRegister(RC522_REG_DIV_IRQ) & 0x04U) == 0U)
    {
        if (HAL_GetTick() - start >= RC522_CRC_TIMEOUT_MS)
            return false;
    }

    result[0] = rc522ReadRegister(RC522_REG_CRC_RESULT_L);
    result[1] = rc522ReadRegister(RC522_REG_CRC_RESULT_H);
    return true;
}

static bool rc522Transceive(const uint8_t *sendData, uint8_t sendLength,
                            uint8_t txLastBits, uint8_t *receiveData,
                            uint8_t *receiveLength, uint8_t *rxLastBits)
{
    /*
     * 카드 명령 송수신의 공통 함수이다.
     * 1) 송신 데이터를 FIFO에 적재
     * 2) Transceive 명령 실행
     * 3) 안테나로 카드에 전송하고 응답 대기
     * 4) 수신 FIFO에서 응답을 복사
     */
    if (sendData == NULL || sendLength == 0U || receiveLength == NULL)
        return false;

    uint8_t capacity = *receiveLength;
    uint8_t irq;

    rc522WriteRegister(RC522_REG_COMMAND, RC522_CMD_IDLE);
    rc522WriteRegister(RC522_REG_COM_IRQ, 0x7FU);
    rc522WriteRegister(RC522_REG_FIFO_LEVEL, 0x80U);

    for (uint8_t i = 0; i < sendLength; i++)
        rc522WriteRegister(RC522_REG_FIFO_DATA, sendData[i]);

    /* REQA처럼 마지막 바이트 중 일부 비트만 보낼 때 txLastBits를 사용한다. */
    rc522WriteRegister(RC522_REG_BIT_FRAMING, (uint8_t)(txLastBits & 0x07U));
    rc522WriteRegister(RC522_REG_COMMAND, RC522_CMD_TRANSCEIVE);
    rc522SetBitMask(RC522_REG_BIT_FRAMING, 0x80U);

    uint32_t start = HAL_GetTick();
    /* RxIRq/IdleIRq가 발생하거나 RC522 내부 타이머가 만료될 때까지 대기한다. */
    do
    {
        irq = rc522ReadRegister(RC522_REG_COM_IRQ);

        if ((irq & 0x01U) != 0U)
        {
            rc522ClearBitMask(RC522_REG_BIT_FRAMING, 0x80U);
            return false;
        }

        if (HAL_GetTick() - start >= RC522_TRANSCEIVE_MS)
        {
            rc522ClearBitMask(RC522_REG_BIT_FRAMING, 0x80U);
            return false;
        }
    }
    while ((irq & 0x30U) == 0U);

    rc522ClearBitMask(RC522_REG_BIT_FRAMING, 0x80U);

    /* 버퍼 오버플로, 패리티, 프로토콜, 충돌 오류가 있으면 실패 처리한다. */
    uint8_t error = rc522ReadRegister(RC522_REG_ERROR);
    if ((error & 0x1BU) != 0U)
        return false;

    uint8_t fifoLength = rc522ReadRegister(RC522_REG_FIFO_LEVEL);
    if (fifoLength > RC522_FIFO_SIZE || fifoLength > capacity)
        return false;

    if (fifoLength > 0U && receiveData == NULL)
        return false;

    for (uint8_t i = 0; i < fifoLength; i++)
        receiveData[i] = rc522ReadRegister(RC522_REG_FIFO_DATA);

    *receiveLength = fifoLength;
    if (rxLastBits != NULL)
        *rxLastBits = (uint8_t)(rc522ReadRegister(RC522_REG_CONTROL) & 0x07U);

    return true;
}

static bool rc522RequestA(void)
{
    /*
     * REQA(0x26)를 7비트로 전송해 안테나 범위 안에 대기 중인
     * ISO 14443-A 카드가 있는지 확인한다. 정상 카드는 ATQA 2바이트를 보낸다.
     */
    uint8_t command = PICC_CMD_REQA;
    uint8_t atqa[2] = {0};
    uint8_t length = sizeof(atqa);
    uint8_t validBits = 0;

    rc522ClearBitMask(RC522_REG_COLL, 0x80U);

    if (!rc522Transceive(&command, 1, 7, atqa, &length, &validBits))
        return false;

    return length == 2U && validBits == 0U;
}

static bool rc522SelectUid(rc522Uid_t *uid)
{
    /*
     * Anti-collision 및 Select 과정으로 카드 UID를 얻는다.
     * UID 길이는 카드에 따라 4/7/10바이트이므로 CL1~CL3를 순서대로 처리한다.
     * 한 단계의 응답은 UID 조각 4바이트 + XOR 검사값(BCC) 1바이트이다.
     */
    static const uint8_t cascadeCommands[3] = {
        PICC_CMD_SEL_CL1,
        PICC_CMD_SEL_CL2,
        PICC_CMD_SEL_CL3
    };

    uid->size = 0;
    uid->sak = 0;

    for (uint8_t level = 0; level < 3U; level++)
    {
        uint8_t anticollision[2] = {cascadeCommands[level], 0x20U};
        uint8_t cascadeData[5] = {0};
        uint8_t cascadeLength = sizeof(cascadeData);
        uint8_t validBits = 0;

        rc522ClearBitMask(RC522_REG_COLL, 0x80U);

        if (!rc522Transceive(anticollision, sizeof(anticollision), 0,
                             cascadeData, &cascadeLength, &validBits))
            return false;

        if (cascadeLength != 5U || validBits != 0U)
            return false;

        /* BCC가 맞지 않으면 UID 응답이 통신 중 손상된 것이다. */
        uint8_t bcc = (uint8_t)(cascadeData[0] ^ cascadeData[1] ^
                                cascadeData[2] ^ cascadeData[3]);
        if (bcc != cascadeData[4])
            return false;

        /* UID 조각을 포함한 SELECT 명령에 CRC_A 2바이트를 붙인다. */
        uint8_t selectBuffer[9] = {
            cascadeCommands[level], 0x70U,
            cascadeData[0], cascadeData[1], cascadeData[2],
            cascadeData[3], cascadeData[4], 0, 0
        };

        if (!rc522CalculateCrc(selectBuffer, 7, &selectBuffer[7]))
            return false;

        uint8_t sakResponse[3] = {0};
        uint8_t sakLength = sizeof(sakResponse);
        if (!rc522Transceive(selectBuffer, sizeof(selectBuffer), 0,
                             sakResponse, &sakLength, &validBits))
            return false;

        if (sakLength != 3U || validBits != 0U)
            return false;

        uint8_t sakCrc[2];
        if (!rc522CalculateCrc(sakResponse, 1, sakCrc))
            return false;
        if (sakCrc[0] != sakResponse[1] || sakCrc[1] != sakResponse[2])
            return false;

        /* CT(0x88)는 UID가 다음 Cascade Level로 이어진다는 표시라 UID에서 제외한다. */
        uint8_t firstUidByte = (cascadeData[0] == PICC_CMD_CT) ? 1U : 0U;
        uint8_t bytesToCopy = (uint8_t)(4U - firstUidByte);

        if ((uint8_t)(uid->size + bytesToCopy) > RC522_UID_MAX_SIZE)
            return false;

        memcpy(&uid->bytes[uid->size], &cascadeData[firstUidByte],
               bytesToCopy);
        uid->size = (uint8_t)(uid->size + bytesToCopy);
        uid->sak = sakResponse[0];

        /* SAK의 Cascade 비트가 0이면 UID를 모두 읽은 것이다. */
        if ((uid->sak & PICC_SAK_CASCADE_BIT) == 0U)
            return true;
    }

    return false;
}

static void rc522HaltA(void)
{
    /* 읽기가 끝난 카드를 HALT 상태로 보내 같은 카드가 계속 감지되는 것을 막는다. */
    uint8_t buffer[4] = {PICC_CMD_HALT_A, 0x00U, 0, 0};
    uint8_t response[1];
    uint8_t responseLength = sizeof(response);

    if (!rc522CalculateCrc(buffer, 2, &buffer[2]))
        return;

    /* HALT 명령에 대한 정상 응답은 타임아웃이므로 결과는 사용하지 않는다. */
    (void)rc522Transceive(buffer, sizeof(buffer), 0,
                          response, &responseLength, NULL);
}

uint8_t rc522GetVersion(void)
{
    /* SPI 배선 확인에 유용하다. 일반적으로 정품/호환 칩은 0x91, 0x92 등을 반환한다. */
    return rc522ReadRegister(RC522_REG_VERSION);
}

bool rc522Init(void)
{
    /* CS 비활성화 후 RST 핀으로 하드웨어 리셋한다. */
    rc522Deselect();

    HAL_GPIO_WritePin(RC522RST_GPIO_Port, RC522RST_Pin, GPIO_PIN_RESET);
    HAL_Delay(2);
    HAL_GPIO_WritePin(RC522RST_GPIO_Port, RC522RST_Pin, GPIO_PIN_SET);
    HAL_Delay(50);

    /* 내부 상태도 소프트웨어 리셋해 이전 동작의 영향을 제거한다. */
    rc522WriteRegister(RC522_REG_COMMAND, RC522_CMD_SOFT_RESET);
    HAL_Delay(50);

    /* ISO 14443-A 통신용 타이머, 변조 방식, CRC 기본값을 설정한다. */
    rc522WriteRegister(RC522_REG_TX_MODE, 0x00U);
    rc522WriteRegister(RC522_REG_RX_MODE, 0x00U);
    rc522WriteRegister(RC522_REG_MOD_WIDTH, 0x26U);
    rc522WriteRegister(RC522_REG_T_MODE, 0x80U);
    rc522WriteRegister(RC522_REG_T_PRESCALER, 0xA9U);
    rc522WriteRegister(RC522_REG_T_RELOAD_H, 0x03U);
    rc522WriteRegister(RC522_REG_T_RELOAD_L, 0xE8U);
    rc522WriteRegister(RC522_REG_TX_ASK, 0x40U);
    rc522WriteRegister(RC522_REG_MODE, 0x3DU);

    /* TxControlReg 하위 2비트를 켜 안테나 드라이버를 활성화한다. */
    if ((rc522ReadRegister(RC522_REG_TX_CONTROL) & 0x03U) != 0x03U)
        rc522SetBitMask(RC522_REG_TX_CONTROL, 0x03U);

    /* 0x00/0xFF는 보통 전원, CS, SCK, MISO 배선 오류를 의미한다. */
    uint8_t version = rc522GetVersion();
    return version != 0x00U && version != 0xFFU;
}

bool rc522ReadUid(rc522Uid_t *uid)
{
    /* 공개 API: 카드 존재 확인 -> UID 선택/복사 -> 카드 HALT 순으로 진행한다. */
    if (uid == NULL)
        return false;

    memset(uid, 0, sizeof(*uid));

    if (!rc522RequestA())
        return false;

    if (!rc522SelectUid(uid))
        return false;

    rc522HaltA();
    return true;
}
