# API Reference

## GET `/api/metrics/general`
Возвращает только **общие метрики майнера**, без данных по вентиляторам и чипам.  

Response:  
```json
{
  "hashrate": 1050,
  "temperature": 68.5,
  "power": 1200,
  "voltage": 12.1,
  "utility": 98.2,
  "accepted": 123456,
  "rejected": 234,
  "hw_errors": 5
}
```

---

## GET `/api/metrics/history`
Возвращает историю метрик за указанный период.  

Request
- `param` (str) — метрика для получения истории (hashrate, temperature, power, voltage, utility и т.д.)  
- `range` (int, optional) — глубина истории в часах (по умолчанию 1)

```json
{
  "param": "hashrate",
  "range": 24
}
```

Response:  
```json
[
  {"time": 1758093600, "value": 1000},
  {"time": 1758097200, "value": 1020},
  {"time": 1758100800, "value": 1050}
]
``` 

---

## GET `/api/metrics/fans`
Возвращает последние показания по вентиляторам:
- номер (id)
- скорость вращения каждого вентилятора (RPM)  

Response:  
```json
[
  {"id": 0, "rpm": 4500},
  {"id": 1, "rpm": 4700}
]
``` 

---

## GET `/api/metrics/chips`
Возвращает последние показания по каждому чипу:  
- номер (id)
- температура  
- частота
- хешрейт (если доступен)  

Response:  
```json
[
  {"id": 0, "temperature": 65.2, "freq": 700, "hashrate": 25.1},
  {"id": 1, "temperature": 66.0, "freq": 710, "hashrate": 25.4}
]
```
