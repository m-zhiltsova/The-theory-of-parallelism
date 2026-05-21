import argparse
import logging
import sys
import threading
import time
import queue
from pathlib import Path

import cv2
import numpy as np


LOG_DIR = Path("log")
LOG_DIR.mkdir(exist_ok=True)
LOG_FILE = LOG_DIR / "sensor_app.log"

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    handlers=[
        logging.FileHandler(LOG_FILE, encoding="utf-8"),
        logging.StreamHandler(sys.stdout)
    ]
)
logger = logging.getLogger("SensorApp")


class Sensor():
    def get(self):
        raise NotImplementedError("Subclasses must implement method get()")


class SensorX(Sensor):
    def __init__(self, delay: float):
        self._delay = delay
        self._data = 0

    def get(self) -> int:
        time.sleep(self._delay)
        self._data += 1
        return self._data


class SensorCam(Sensor):
    MAX_CONSECUTIVE_ERRORS = 10  

    def __init__(self, camera_name: str, resolution: str):
        self._camera_name = camera_name
        self._resolution = resolution
        self._cap = None
        self._width, self._height = self._parse_resolution(resolution)
        self._consecutive_errors = 0
        self._open()

    def _parse_resolution(self, res_str: str) -> tuple:
        try:
            w, h = map(int, res_str.lower().split('x'))
            return w, h
        except Exception:
            logger.error("Invalid resolution format: %s. Use WxH (e.g. 1280x720)", res_str)
            sys.exit(1)

    def _open(self):
        self._cap = cv2.VideoCapture(self._camera_name)
        if not self._cap.isOpened():
            logger.error("Cannot open camera %s", self._camera_name)
            sys.exit(1)

        self._cap.set(cv2.CAP_PROP_FRAME_WIDTH, self._width)
        self._cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self._height)

        actual_w = self._cap.get(cv2.CAP_PROP_FRAME_WIDTH)
        actual_h = self._cap.get(cv2.CAP_PROP_FRAME_HEIGHT)
        if (actual_w, actual_h) != (self._width, self._height):
            logger.warning(
                "Desired resolution %dx%d not supported, using %dx%d",
                self._width, self._height, int(actual_w), int(actual_h)
            )
            self._width, self._height = int(actual_w), int(actual_h)

    def get(self):
        ret, frame = self._cap.read()
        if not ret:
            self._consecutive_errors += 1
            logger.error(
                "Failed to read frame from camera %s (error %d/%d)",
                self._camera_name, self._consecutive_errors, self.MAX_CONSECUTIVE_ERRORS
            )
            if self._consecutive_errors >= self.MAX_CONSECUTIVE_ERRORS:
                logger.critical("Too many consecutive camera errors. Exiting.")
                sys.exit(1)
            return None
        else:
            self._consecutive_errors = 0
            return frame

    def __del__(self):
        if self._cap is not None:
            self._cap.release()
            logger.info("Camera %s released.", self._camera_name)


class WindowImage:
    WINDOW_NAME = "Sensor Display"

    def __init__(self, fps: float):
        self._fps = fps
        cv2.namedWindow(self.WINDOW_NAME, cv2.WINDOW_NORMAL)
        cv2.resizeWindow(self.WINDOW_NAME, 800, 600)
        logger.info("Display window created (target FPS: %.1f)", fps)

    def show(self, img):
        cv2.imshow(self.WINDOW_NAME, img)

    def wait_key(self, delay_ms: int = 1):
        return cv2.waitKey(delay_ms)

    def __del__(self):
        cv2.destroyWindow(self.WINDOW_NAME)
        logger.info("Display window destroyed.")


def put_latest(q: queue.Queue, data):
    try:
        while True:
            q.get_nowait()
    except queue.Empty:
        pass
    q.put(data)


def sensor_worker(sensor: Sensor, data_queue: queue.Queue, stop_event: threading.Event):
    logger.info("Sensor worker started for %s", type(sensor).__name__)
    while not stop_event.is_set():
        try:
            value = sensor.get()
            put_latest(data_queue, value)
        except Exception as e:
            logger.exception("Exception in sensor worker: %s", e)
    logger.info("Sensor worker stopped for %s", type(sensor).__name__)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--camera", default="/dev/video0")
    parser.add_argument("--resolution", default="1280x720")
    parser.add_argument("--freq", type=float, default=30.0)
    args = parser.parse_args()

    target_fps = max(1.0, args.freq)
    frame_period = 1.0 / target_fps

    try:
        cam_sensor = SensorCam(args.camera, args.resolution)
    except SystemExit:
        raise
    sensor0 = SensorX(0.01)
    sensor1 = SensorX(0.1)
    sensor2 = SensorX(1.0)

    sensors = [
        ("Cam", cam_sensor),
        ("X0 (100Hz)", sensor0),
        ("X1 (10Hz)", sensor1),
        ("X2 (1Hz)", sensor2)
    ]

    queues = {
        name: queue.Queue(maxsize=1) for name, _ in sensors
    }

    latest_data = {
        name: None for name, _ in sensors
    }

    stop_event = threading.Event()

    threads = []
    for name, sensor in sensors:
        t = threading.Thread(
            target=sensor_worker,
            args=(sensor, queues[name], stop_event),
            name=f"Worker-{name}",
            daemon=True
        )
        t.start()
        threads.append(t)

    window = WindowImage(target_fps)

    logger.info("Application started. Press 'q' to quit.")
    try:
        while True:
            start_time = time.monotonic()

            for name, q in queues.items():
                try:
                    new_val = q.get_nowait()
                    latest_data[name] = new_val
                except queue.Empty:
                    pass

            cam_frame = latest_data["Cam"]
            if cam_frame is None:
                cam_frame = np.zeros((480, 640, 3), dtype=np.uint8)

            img = cam_frame.copy()
            y_offset = 30
            for i, (name, _) in enumerate(sensors):
                val = latest_data[name]
                text = f"{name}: {val}"
                cv2.putText(img, text, (10, y_offset + i * 30),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

            window.show(img)

            key = window.wait_key(1) & 0xFF
            if key == ord('q'):
                logger.info("Quit key pressed.")
                break

            elapsed = time.monotonic() - start_time
            sleep_time = frame_period - elapsed
            if sleep_time > 0:
                time.sleep(sleep_time)

    except KeyboardInterrupt:
        logger.info("Interrupted by user.")
    finally:
        logger.info("Shutting down...")
        stop_event.set()
        for t in threads:
            t.join(timeout=2.0)
        cv2.destroyAllWindows()
        logger.info("Application stopped.")


if __name__ == "__main__":
    main()
