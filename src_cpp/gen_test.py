import random
import os

words = ['INFO', 'WARN', 'ERROR', 'DEBUG', 'server', 'request', 'response',
         'user', 'session', 'database', 'query', 'timeout', 'connect',
         'close', 'write', 'read', 'GET', 'POST', '200', '404', '500']

lines = []
for i in range(100000):
    ts = '2024-01-{:02d} {:02d}:{:02d}:{:02d}'.format(
        (i % 28) + 1, (i // 3600) % 24, (i // 60) % 60, i % 60)
    level = random.choice(['INFO ', 'WARN ', 'ERROR'])
    msg = ' '.join(random.choices(words, k=random.randint(3, 8)))
    lines.append('{} [{}] {}'.format(ts, level, msg))

with open('test_5mb.txt', 'w') as f:
    f.write('\n'.join(lines))

size = os.path.getsize('test_5mb.txt')
print('Size: {:.2f} MB'.format(size / 1024 / 1024))
