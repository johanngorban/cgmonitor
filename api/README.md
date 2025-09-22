# API Reference

## GET `/metrics/current`
Возвращает последние собранные метрики майнера **со всеми данными**:  
- общие показатели устройства  
- данные вентиляторов  
- данные чипов  

Response:  
```json
{
  "time": "2025-09-17T12:34:56",
  "hashrate": 1050,
  "temperature": 68.5,
  "power": 1200,
  "voltage": 12.1,
  "utility": 98.2,
  "accepted": 123456,
  "rejected": 234,
  "hw_errors": 5,
  "fans": [
    {"id": 0, "rpm": 4500},
    {"id": 1, "rpm": 4700}
  ],
  "chips": [
    {"id": 0, "temperature": 65.2, "freq": 700, "hashrate": 25.1},
    {"id": 1, "temperature": 66.0, "freq": 710, "hashrate": 25.4}
  ]
}
```

---

## GET `/metrics/general`
Возвращает только **общие метрики майнера**, без данных по вентиляторам и чипам.  

Response:  
```json
{
  "time": "2025-09-17T12:34:56",
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

## GET `/metrics/history`
Возвращает историю метрик за указанный период.  

Query parameters:  
- `param` (str) — метрика для получения истории (hashrate, temperature, power, voltage, utility и т.д.)  
- `last_hours` (int, optional) — глубина истории в часах (по умолчанию 24)  
- `points` (int, optional) — количество точек (по умолчанию 500)  

Response:  
```json
{
  "last_hours": 24,
  "points": 3,
  "data": {
    "hashrate": [
      {"time": "2025-09-17T10:00:00", "value": 1000},
      {"time": "2025-09-17T11:00:00", "value": 1020},
      {"time": "2025-09-17T12:00:00", "value": 1050}
    ],
    "temperature": [
      {"time": "2025-09-17T10:00:00", "value": 65.3},
      {"time": "2025-09-17T11:00:00", "value": 66.1},
      {"time": "2025-09-17T12:00:00", "value": 68.5}
    ]
  }
}
``` 

---

## GET `/metrics/summary`
Возвращает агрегированную статистику по основным метрикам за указанный период.  

Query parameters:  
- `last_hours` (int, optional) — глубина анализа (по умолчанию 24)  

Response:  
```json
{
  "last_hours": 24,
  "summary": {
    "hashrate": {"min": 950, "max": 1100, "avg": 1025},
    "temperature": {"min": 60.1, "max": 72.3, "avg": 66.5},
    "power": {"min": 1180, "max": 1230, "avg": 1205},
    "fan_rpm": {"min": 4400, "max": 4800, "avg": 4600}
  }
}
``` 

---

## GET `/metrics/fans`
Возвращает последние показания по вентиляторам:  
- скорость вращения каждого вентилятора (RPM)  
- состояние  

Response:  
```json
[
  {"id": 0, "rpm": 4500},
  {"id": 1, "rpm": 4700}
]
``` 

---

## GET `/metrics/chips`
Возвращает последние показания по каждому чипу:  
- температура  
- хешрейт (если доступен)  
- состояние  

Response:  
```json
[
  {"id": 0, "temperature": 65.2, "freq": 700, "hashrate": 25.1},
  {"id": 1, "temperature": 66.0, "freq": 710, "hashrate": 25.4}
]
```
