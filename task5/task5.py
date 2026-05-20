import argparse
import logging
import os
import queue
import sys
import threading
import time
from pathlib import Path

import cv2
import numpy as np
from ultralytics import YOLO

LOG_DIR = Path("log")
LOG_DIR.mkdir(exist_ok=True)
LOG_FILE = LOG_DIR / "yolo_pose_video.log"

logging.basicConfig(
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(name)s: %(message)s",
    handlers=[
        logging.FileHandler(LOG_FILE, encoding="utf-8"),
        logging.StreamHandler(sys.stdout),
    ],
)
logger = logging.getLogger("YOLO-Pose-Video")


class VideoReader:
    def __init__(self, video_path: str):
        self.video_path = video_path
        self.cap = cv2.VideoCapture(video_path)
        if not self.cap.isOpened():
            logger.error("Не удалось открыть видеофайл: %s", video_path)
            raise RuntimeError(f"Cannot open video: {video_path}")

        self.fps = self.cap.get(cv2.CAP_PROP_FPS)
        self.width = int(self.cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        self.height = int(self.cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        self.total_frames = int(self.cap.get(cv2.CAP_PROP_FRAME_COUNT))

        logger.info(
            "Видео открыто: %s, разрешение: %dx%d, fps: %.2f, кадров: %d",
            video_path,
            self.width,
            self.height,
            self.fps,
            self.total_frames,
        )

    def read_frame(self):
        return self.cap.read()

    def release(self):
        if self.cap is not None:
            self.cap.release()
            logger.info("Видеозахват освобождён.")

    def __del__(self):
        self.release()


class VideoWriter:
    def __init__(self, output_path: str, fps: float, width: int, height: int):
        self.output_path = output_path
        self.fps = fps
        self.width = width
        self.height = height
        fourcc = cv2.VideoWriter_fourcc(*"mp4v")
        self.writer = cv2.VideoWriter(output_path, fourcc, fps, (width, height))
        if not self.writer.isOpened():
            logger.error("Не удалось создать выходной видеофайл: %s", output_path)
            raise RuntimeError(f"Cannot create video writer: {output_path}")
        logger.info("Выходной видеофайл создан: %s", output_path)

    def write_frame(self, frame):
        self.writer.write(frame)

    def release(self):
        if self.writer is not None:
            self.writer.release()
            logger.info("Видеозапись освобождена.")

    def __del__(self):
        self.release()


def worker_thread(
    model_path: str,
    input_queue: queue.Queue,
    output_queue: queue.Queue,
    stop_event: threading.Event,
):
    try:
        model = YOLO(model_path)
        logger.debug("Поток %s: модель YOLO загружена.", threading.current_thread().name)
    except Exception as e:
        logger.exception("Ошибка загрузки модели YOLO в потоке %s", threading.current_thread().name)
        return

    while not stop_event.is_set():
        try:
            frame_idx, frame = input_queue.get(timeout=1)
        except queue.Empty:
            continue

        try:
            results = model.predict(frame, verbose=False, stream=False)
            annotated_frame = results[0].plot()
            output_queue.put((frame_idx, annotated_frame))
        except Exception as e:
            logger.exception("Ошибка инференса в потоке %s для кадра %d",
                             threading.current_thread().name, frame_idx)
            error_frame = frame.copy()
            cv2.putText(error_frame, "ERROR", (50, 50),
                        cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
            output_queue.put((frame_idx, error_frame))
        finally:
            input_queue.task_done()


def process_video_single(video_reader, video_writer, model_path):
    model = YOLO(model_path)
    logger.info("Запущена однопоточная обработка.")

    start_time = time.perf_counter()
    frame_idx = 0

    while True:
        ret, frame = video_reader.read_frame()
        if not ret:
            break

        try:
            results = model.predict(frame, verbose=False, stream=False)
            annotated_frame = results[0].plot()
        except Exception as e:
            logger.exception("Ошибка инференса на кадре %d", frame_idx)
            annotated_frame = frame.copy()
            cv2.putText(annotated_frame, "ERROR", (50, 50),
                        cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)

        video_writer.write_frame(annotated_frame)
        frame_idx += 1

    elapsed = time.perf_counter() - start_time
    logger.info("Однопоточная обработка завершена. Кадров: %d, время: %.2f сек.", frame_idx, elapsed)
    return elapsed


def process_video_multi(video_reader, video_writer, model_path, num_threads):
    logger.info("Запущена многопоточная обработка (%d потоков).", num_threads)

    input_queue = queue.Queue(maxsize=num_threads * 2)
    output_queue = queue.Queue()

    stop_event = threading.Event()

    threads = []
    for i in range(num_threads):
        t = threading.Thread(
            target=worker_thread,
            args=(model_path, input_queue, output_queue, stop_event),
            name=f"Worker-{i}",
            daemon=True,
        )
        t.start()
        threads.append(t)

    def frame_reader():
        frame_idx = 0
        while True:
            ret, frame = video_reader.read_frame()
            if not ret:
                break
            while not stop_event.is_set():
                try:
                    input_queue.put((frame_idx, frame), timeout=1)
                    break
                except queue.Full:
                    continue
            frame_idx += 1
        logger.debug("Чтение кадров завершено. Всего прочитано: %d", frame_idx)

    reader_thread = threading.Thread(target=frame_reader, name="FrameReader")
    reader_thread.start()

    start_time = time.perf_counter()
    frame_idx = 0
    pending_frames = {}

    while True:
        if not reader_thread.is_alive() and output_queue.empty():
            if not pending_frames:
                break

        try:
            idx, annotated_frame = output_queue.get(timeout=1)
        except queue.Empty:
            continue

        pending_frames[idx] = annotated_frame

        while frame_idx in pending_frames:
            video_writer.write_frame(pending_frames.pop(frame_idx))
            frame_idx += 1

    elapsed = time.perf_counter() - start_time

    stop_event.set()
    reader_thread.join(timeout=5)
    for t in threads:
        t.join(timeout=5)

    logger.info("Многопоточная обработка завершена. Кадров: %d, время: %.2f сек.", frame_idx, elapsed)
    return elapsed


def main():
    parser = argparse.ArgumentParser(
        description="Обработка видео с помощью YOLOv8s-pose (однопоточный/многопоточный режим)"
    )
    parser.add_argument(
        "--input", type=str, required=True,
        help="Путь к исходному видеофайлу (рекомендуется 640x480)",
    )
    parser.add_argument(
        "--mode", type=str, choices=["single", "multi"], default="multi",
        help="Режим выполнения: single – однопоточный, multi – многопоточный",
    )
    parser.add_argument(
        "--output", type=str, default="output_pose.mp4",
        help="Имя выходного видеофайла",
    )
    parser.add_argument(
        "--threads", type=int, default=4,
        help="Количество потоков для многопоточного режима (по умолчанию 4)",
    )
    parser.add_argument(
        "--model", type=str, default="yolov8s-pose.pt",
        help="Путь к весам модели YOLO (по умолчанию yolov8s-pose.pt)",
    )
    args = parser.parse_args()

    if not os.path.exists(args.input):
        logger.error("Входной видеофайл не найден: %s", args.input)
        sys.exit(1)

    try:
        reader = VideoReader(args.input)
    except RuntimeError:
        sys.exit(1)

    writer = VideoWriter(
        args.output,
        reader.fps,
        reader.width,
        reader.height,
    )

    try:
        if args.mode == "single":
            elapsed = process_video_single(reader, writer, args.model)
        else:
            elapsed = process_video_multi(reader, writer, args.model, args.threads)

        print(f"Время обработки: {elapsed:.2f} сек.")
        print(f"Выходной файл сохранён: {args.output}")

    except Exception as e:
        logger.exception("Неожиданная ошибка во время обработки.")
        sys.exit(1)
    finally:
        reader.release()
        writer.release()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()
