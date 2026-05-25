#include "adapters/display/Gb2312FontFlash.h"
#include "adapters/display/SharedSpiBus.h"
#include "config/Config.h"

namespace WallE {

namespace {
/// 中文：SPI Flash 读取数据命令。
/// English: SPI flash read-data command.
constexpr uint8_t kReadData = 0x03;

/// 中文：退出低功耗/唤醒命令。
/// English: Release-from-power-down / wake-up command.
constexpr uint8_t kReleasePowerDown = 0xAB;

/// 中文：8x16 ASCII 字库在 Flash 中的起始地址。
/// English: Start address of the 8x16 ASCII font region in flash.
constexpr uint32_t kAscii16Base = 0x1D00;

/// 中文：16x16 GB2312 汉字字库在 Flash 中的起始地址。
/// English: Start address of the 16x16 GB2312 Chinese font region in flash.
constexpr uint32_t kWord16Base = 0x71300;

/// 中文：一个 8x16 ASCII 字符占用 16 字节。
/// English: One 8x16 ASCII glyph uses 16 bytes.
constexpr size_t kAscii16Size = 16;

/// 中文：一个 16x16 汉字占用 32 字节。
/// English: One 16x16 Chinese glyph uses 32 bytes.
constexpr size_t kWord16Size = 32;

/// 中文：字库镜像签名，用于确认外置 Flash 中烧录的是正确资源。
/// English: Font image signature used to verify the external flash contains the expected resource.
const uint8_t kSignature[] = {'J', 'Y', 'C', '-', '4', 'M', 'b', 'B', 'y', 't', 'e', '-',
                              'F', 'O', 'N', 'T', '-', 'F', 'L', 'A', 'S', 'H'};
}  // namespace

/**
 * 中文：准备字库 Flash：配置 CS、初始化 SPI，并发送 0xAB 唤醒命令。
 * English: Prepares the font flash by configuring CS/SPI and sending the 0xAB wake command.
 *
 * @param 无 / None.
 * @return 无 / None.
 */
void Gb2312FontFlash::begin() {
  pinMode(WallEConfig::kFontCs, OUTPUT);
  deselect();
  SPI.begin(WallEConfig::kTftSck, WallEConfig::kTftMiso, WallEConfig::kTftMosi);

  select();
  SPI.transfer(kReleasePowerDown);
  SPI.transfer(0x00);
  SPI.transfer(0x00);
  SPI.transfer(0x00);
  SPI.transfer(0x00);
  deselect();
  delay(2);
}

/**
 * 中文：读取 Flash 地址 0 的签名，确认外置字库数据是否正确。
 * English: Reads the signature at flash address 0 to verify the external font image.
 *
 * @param 无 / None.
 * @return 中文：签名和 kSignature 完全一致返回 true。
 *         English: true when the signature exactly matches kSignature.
 */
bool Gb2312FontFlash::checkSignature() {
  // 中文：data 临时保存从 Flash 读取到的签名字节。
  // English: data temporarily stores the signature bytes read from flash.
  uint8_t data[sizeof(kSignature)] = {0};
  read(0, data, sizeof(data));
  return memcmp(data, kSignature, sizeof(kSignature)) == 0;
}

/**
 * 中文：执行 SPI Flash 标准 0x03 读命令，从指定地址连续读取字节。
 * English: Executes the standard SPI flash 0x03 read command and reads continuous bytes from an address.
 *
 * @param address 中文：Flash 24 位地址。
 *                English: 24-bit flash address.
 * @param buffer 中文：接收数据缓冲区；为空时直接返回。
 *               English: Destination buffer; returns immediately when null.
 * @param length 中文：读取长度；为 0 时直接返回。
 *               English: Number of bytes to read; returns immediately when 0.
 * @return 无 / None.
 */
void Gb2312FontFlash::read(uint32_t address, uint8_t* buffer, size_t length) {
  if (buffer == nullptr || length == 0) {
    return;
  }

  select();
  SPI.transfer(kReadData);
  SPI.transfer((address >> 16) & 0xFF);
  SPI.transfer((address >> 8) & 0xFF);
  SPI.transfer(address & 0xFF);
  for (size_t i = 0; i < length; ++i) {
    buffer[i] = SPI.transfer(0x00);
  }
  deselect();
}

/**
 * 中文：按 ASCII 编码计算字库地址并读取 8x16 点阵。
 * English: Computes the font address from an ASCII code and reads its 8x16 bitmap.
 *
 * @param ascii 中文：ASCII 编码值。
 *              English: ASCII code value.
 * @param buffer 中文：接收 16 字节点阵数据的缓冲区。
 *               English: Destination buffer for 16 bitmap bytes.
 * @return 无 / None.
 */
void Gb2312FontFlash::readAscii16(uint8_t ascii, uint8_t* buffer) {
  read(kAscii16Base + ascii * kAscii16Size, buffer, kAscii16Size);
}

/**
 * 中文：按 GB2312 双字节编码计算字库地址并读取 16x16 点阵。
 * English: Computes the font address from a GB2312 two-byte code and reads its 16x16 bitmap.
 *
 * @param high 中文：GB2312 高字节。
 *             English: GB2312 high byte.
 * @param low 中文：GB2312 低字节。
 *            English: GB2312 low byte.
 * @param buffer 中文：接收 32 字节点阵数据的缓冲区。
 *               English: Destination buffer for 32 bitmap bytes.
 * @return 无 / None.
 */
void Gb2312FontFlash::readWord16(uint8_t high, uint8_t low, uint8_t* buffer) {
  // 中文：GB2312 区位码每区 94 个字符，高低字节都从 0xA1 起算。
  // English: GB2312 has 94 glyphs per area; both bytes are based at 0xA1.
  const uint32_t index = (static_cast<uint32_t>(high - 0xA1) * 94) + (low - 0xA1);
  read(kWord16Base + index * kWord16Size, buffer, kWord16Size);
}

/**
 * 中文：独占 SPI 总线并拉低字库 Flash CS。
 * English: Takes the SPI bus and pulls the font flash CS low.
 *
 * @param 无 / None.
 * @return 无 / None.
 */
void Gb2312FontFlash::select() {
  deselectSharedSpiDevices();
  digitalWrite(WallEConfig::kFontCs, LOW);
  SPI.beginTransaction(SPISettings(WallEConfig::kFontSpiHz, MSBFIRST, SPI_MODE0));
}

/**
 * 中文：结束 SPI 事务并释放字库 Flash CS。
 * English: Ends the SPI transaction and releases the font flash CS.
 *
 * @param 无 / None.
 * @return 无 / None.
 */
void Gb2312FontFlash::deselect() {
  SPI.endTransaction();
  digitalWrite(WallEConfig::kFontCs, HIGH);
}

}  // namespace WallE
