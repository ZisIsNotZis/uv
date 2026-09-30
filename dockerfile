FROM python
RUN pip install -ihttps://mirror.baidu.com/pypi/simple uvicorn[standard] && rm -r ~/.cache
COPY . .
