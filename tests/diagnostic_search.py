import urllib.request
import json
import sys

url = "http://172.16.50.14/DHAKA-FLIX-14/"
payload = {
    "action": "get",
    "search": {
        "href": "/DHAKA-FLIX-14/",
        "pattern": "kingdom",
        "ignorecase": True
    }
}

req_body = json.dumps(payload)
headers = {"Content-Type": "application/json;charset=utf-8"}
req = urllib.request.Request(url, data=req_body.encode("utf-8"), headers=headers, method="POST")

try:
    with urllib.request.urlopen(req, timeout=15) as resp:
        raw_bytes = resp.read()
        raw_json = json.loads(raw_bytes.decode("utf-8"))
except Exception as e:
    print(f"Error fetching: {e}")
    sys.exit(1)

search_results = raw_json.get("search", [])

print("=== 1. EXACT POST URL ===")
print(url)
print("\n=== 2. EXACT JSON REQUEST BODY ===")
print(req_body)
print("\n=== 3. NUMBER OF RAW RESULTS IN response['search'] ===")
print(len(search_results))

print("\n=== 4. FIRST 10 RAW href VALUES ===")
for i, item in enumerate(search_results[:10]):
    print(f"[{i+1}] {item.get('href')}")

print("\n=== 5. LAST 10 RAW href VALUES ===")
start_idx = max(0, len(search_results) - 10)
for i, item in enumerate(search_results[start_idx:], start=start_idx):
    print(f"[{i+1}] {item.get('href')}")

print("\n=== COMPLETE RAW OBJECT SAMPLE (FIRST ITEM) ===")
if search_results:
    print(json.dumps(search_results[0], indent=2))
