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
import torch
from ultralytics import YOLO

import multiprocessing as mp

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
    """Чтение видео с диска (RAII)."""
    def __init__(self, video_path: str):
        self.video_path = video_path
        self.cap = cv2.VideoCapture(video_path)
        if not self.cap.isOpened():
            raise RuntimeError(f"Cannot open video: {video_path}")
        self.fps = self.cap.get(cv2.CAP_PROP_FPS)
        self.width = int(self.cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        self.height = int(self.cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        self.total_frames = int(self.cap.get(cv2.CAP_PROP_FRAME_COUNT))
        logger.info("Video opened: %s, %dx%d, %.2f fps, %d frames",
                    video_path, self.width, self.height, self.fps, self.total_frames)

    def read_frame(self):
        return self.cap.read()

    def release(self):
        if self.cap is not None:
            self.cap.release()

    def __del__(self):
        self.release()


class VideoWriter:
    def __init__(self, output_path: str, fps: float, width: int, height: int):
        fourcc = cv2.VideoWriter_fourcc(*"mp4v")
        self.writer = cv2.VideoWriter(output_path, fourcc, fps, (width, height))
        if not self.writer.isOpened():
            raise RuntimeError(f"Cannot create video writer: {output_path}")
        logger.info("Output video writer created: %s", output_path)

    def write_frame(self, frame):
        self.writer.write(frame)

    def release(self):
        if self.writer is not None:
            self.writer.release()

    def __del__(self):
        self.release()


def process_video_single(reader, writer, model_path):
    model = YOLO(model_path)
    logger.info("Single-threaded processing started.")
    start_time = time.perf_counter()
    frame_idx = 0
    while True:
        ret, frame = reader.read_frame()
        if not ret:
            break
        try:
            results = model(frame, verbose=False)
            annotated = results[0].plot()
        except Exception:
            logger.exception("Error on frame %d", frame_idx)
            annotated = frame.copy()
            cv2.putText(annotated, "ERROR", (50, 50),
                        cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
        writer.write_frame(annotated)
        frame_idx += 1
    elapsed = time.perf_counter() - start_time
    logger.info("Single-threaded: %d frames in %.2f s", frame_idx, elapsed)
    return elapsed


def process_video_threads(reader, writer, model_path, num_workers):
    logger.info("Multi-threaded processing with %d threads.", num_workers)
    input_queue = queue.Queue(maxsize=num_workers * 2)
    output_queue = queue.Queue()
    stop_event = threading.Event()

    def worker():
        torch.set_num_threads(1)
        model = YOLO(model_path)
        while not stop_event.is_set():
            try:
                idx, frame = input_queue.get(timeout=1)
            except queue.Empty:
                continue
            try:
                results = model(frame, verbose=False)
                annotated = results[0].plot()
            except Exception:
                logger.exception("Error in worker for frame %d", idx)
                annotated = frame.copy()
                cv2.putText(annotated, "ERROR", (50, 50),
                            cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
            output_queue.put((idx, annotated))
            input_queue.task_done()

    threads = [threading.Thread(target=worker, name=f"W-{i}", daemon=True)
               for i in range(num_workers)]
    for t in threads:
        t.start()

    def frame_reader():
        idx = 0
        while True:
            ret, frame = reader.read_frame()
            if not ret:
                break
            while not stop_event.is_set():
                try:
                    input_queue.put((idx, frame), timeout=1)
                    break
                except queue.Full:
                    continue
            idx += 1
        logger.debug("Frame reading finished, total %d frames.", idx)

    reader_thread = threading.Thread(target=frame_reader, name="Reader")
    reader_thread.start()

    start_time = time.perf_counter()
    frame_idx = 0
    pending = {}
    while True:
        if not reader_thread.is_alive() and output_queue.empty() and not pending:
            break
        try:
            idx, annotated = output_queue.get(timeout=1)
        except queue.Empty:
            continue
        pending[idx] = annotated
        while frame_idx in pending:
            writer.write_frame(pending.pop(frame_idx))
            frame_idx += 1

    elapsed = time.perf_counter() - start_time
    stop_event.set()
    reader_thread.join(timeout=5)
    for t in threads:
        t.join(timeout=5)
    logger.info("Multi-threaded: %d frames in %.2f s", frame_idx, elapsed)
    return elapsed


def process_video_processes(reader, writer, model_path, num_workers):
    logger.info("Multi-process processing with %d workers.", num_workers)

    input_queue = mp.Queue(maxsize=num_workers * 2)
    output_queue = mp.Queue()

    def worker_process(in_q, out_q):
        torch.set_num_threads(1)
        model = YOLO(model_path)
        logger.info(f"Worker process {os.getpid()} started.")
        while True:
            item = in_q.get()
            if item is None:
                break
            idx, frame = item
            try:
                results = model(frame, verbose=False)
                annotated = results[0].plot()
            except Exception:
                logger.exception("Process worker error for frame %d", idx)
                annotated = frame.copy()
                cv2.putText(annotated, "ERROR", (50, 50),
                            cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
            out_q.put((idx, annotated))

    processes = []
    for i in range(num_workers):
        p = mp.Process(target=worker_process, args=(input_queue, output_queue),
                       name=f"Proc-{i}")
        p.start()
        processes.append(p)

    def send_frames():
        idx = 0
        while True:
            ret, frame = reader.read_frame()
            if not ret:
                break
            input_queue.put((idx, frame))
            idx += 1
        for _ in range(num_workers):
            input_queue.put(None)

    sender = threading.Thread(target=send_frames, name="FrameSender")
    sender.start()

    start_time = time.perf_counter()
    frame_idx = 0
    pending = {}
    finished_workers = 0

    while finished_workers < num_workers or not output_queue.empty():
        try:
            item = output_queue.get(timeout=0.5)
            if item is None:
                continue
            idx, annotated = item
            pending[idx] = annotated
            while frame_idx in pending:
                writer.write_frame(pending.pop(frame_idx))
                frame_idx += 1
        except queue.Empty:
            pass
        for p in processes:
            if not p.is_alive() and p.exitcode is not None and p not in [pp for pp in processes if pp.is_alive()]:
                pass
        if not sender.is_alive() and output_queue.empty() and input_queue.empty():
            all_exited = all(not p.is_alive() for p in processes)
            if all_exited:
                break
        alive = any(p.is_alive() for p in processes)
        if not alive and output_queue.empty() and not pending:
            break

    elapsed = time.perf_counter() - start_time

    for p in processes:
        p.join(timeout=5)

    sender.join(timeout=5)
    logger.info("Multi-process: %d frames in %.2f s", frame_idx, elapsed)
    return elapsed

def main():
    parser = argparse.ArgumentParser(
        description="Инференс YOLOv8s-pose на видео (single / threads / processes)")
    parser.add_argument("--input", required=True, help="Путь к входному видео")
    parser.add_argument("--output", default="output_pose.mp4", help="Выходной видеофайл")
    parser.add_argument("--mode", choices=["single", "threads", "processes"],
                        default="threads", help="Режим параллелизма")
    parser.add_argument("--workers", type=int, default=4,
                        help="Количество потоков или процессов")
    parser.add_argument("--model", default="yolov8s-pose.pt",
                        help="Путь к весам YOLO")
    args = parser.parse_args()

    if not os.path.exists(args.input):
        logger.error("Input video not found: %s", args.input)
        sys.exit(1)

    reader = VideoReader(args.input)
    writer = VideoWriter(args.output, reader.fps, reader.width, reader.height)

    try:
        if args.mode == "single":
            elapsed = process_video_single(reader, writer, args.model)
        elif args.mode == "threads":
            elapsed = process_video_threads(reader, writer, args.model, args.workers)
        else:
            mp.set_start_method('spawn', force=True)
            elapsed = process_video_processes(reader, writer, args.model, args.workers)

        print(f"Режим: {args.mode}, время обработки: {elapsed:.2f} сек.")
        print(f"Результат сохранён в: {args.output}")

    except Exception:
        logger.exception("Unhandled exception during processing.")
        sys.exit(1)
    finally:
        reader.release()
        writer.release()
        cv2.destroyAllWindows()


if __name__ == "__main__":
    main()