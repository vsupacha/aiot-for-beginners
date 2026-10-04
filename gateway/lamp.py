"""กฎของโคมไฟ: ตัดสินใจเปิด/ปิดจากความสว่างและโหมดที่ผู้ใช้เลือก"""

from dataclasses import dataclass


@dataclass(frozen=True)
class LampPolicy:
    """ค่าเริ่มต้น on_below == off_above คือเกณฑ์เดียว (35%) แล้วไฟอาจกะพริบเมื่อแสงอยู่ใกล้เกณฑ์

    ตั้งให้ on_below < off_above (เช่น 30 และ 40) เพื่อให้เกิด hysteresis: ในช่วงกลางไฟคงสถานะเดิม
    """
    on_below: int = 35   # ความสว่าง (%) ต่ำกว่าค่านี้ -> เปิดไฟ
    off_above: int = 35  # ความสว่าง (%) ตั้งแต่ค่านี้ขึ้นไป -> ปิดไฟ

    def __post_init__(self):
        if not 0 <= self.on_below <= self.off_above <= 100:
            raise ValueError("ต้องมี 0 <= on_below <= off_above <= 100")

    def decide(self, light_pct, mode, previous_action=None):
        """คืน (action, reason)"""
        if mode == "on":
            return "LED_ON", "ผู้ใช้สั่งเปิดไฟ"
        if mode == "off":
            return "LED_OFF", "ผู้ใช้สั่งปิดไฟ"
        if light_pct < self.on_below:
            return "LED_ON", f"แสงน้อย ({light_pct}% ต่ำกว่า {self.on_below}%) จึงเปิดไฟ"
        if light_pct >= self.off_above:
            return "LED_OFF", f"แสงเพียงพอ ({light_pct}% ตั้งแต่ {self.off_above}% ขึ้นไป) จึงปิดไฟ"
        # ช่วงกลางของ hysteresis: คงสถานะเดิม
        if previous_action == "LED_ON":
            return "LED_ON", f"แสงอยู่ในช่วงกลาง ({light_pct}%) จึงคงไฟไว้ที่เปิด"
        return "LED_OFF", f"แสงอยู่ในช่วงกลาง ({light_pct}%) จึงคงไฟไว้ที่ปิด"
