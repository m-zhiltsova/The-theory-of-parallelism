python -m venv .venv
source .venv/bin/activate
pip install -r requirements.txt

Запуск в однопоточном режиме
python task5.py --input video.mp4 --mode single --output out_single.mp4

Запуск многопоточном режиме
python task5.py --input video.mp4 --mode multi --threads 8 --output out_multi.mp4
