"""IDA v2 A2A client. Standard library only; no credentials or tokens on disk."""
from __future__ import annotations

import hashlib
import json
import queue
import re
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
import uuid

VERSION = '1.0.0'
DOC_ROOT = 'https://docs.volcengine.com/docs/85637/'
DOCS = {
    'overview': '2649370', 'authentication': '2649373', 'domain': '2649374',
    'create_session': '2649393', 'session_info': '2649388',
    'stream': '2649378', 'subscribe': '2649386', 'task': '2649379', 'cancel': '2649383',
}
TERMINAL = {'TASK_STATE_COMPLETED', 'TASK_STATE_FAILED', 'TASK_STATE_CANCELED', 'TASK_STATE_REJECTED'}
ACTIVE = {'TASK_STATE_SUBMITTED', 'TASK_STATE_WORKING', 'TASK_STATE_INPUT_REQUIRED'}


class IdaError(Exception):
    def __init__(self, code, message, **details):
        super().__init__(message)
        self.code, self.details = code, details


class Redactor:
    def __init__(self):
        self.values = set()

    def add(self, *values):
        self.values.update(str(x) for x in values if x)

    def clean(self, value):
        if isinstance(value, dict):
            return {k: '[REDACTED]' if re.search(r'(secret|password|jwtToken|accessToken|authorization|clientId|proxyUser)', k, re.I)
                    else self.clean(v) for k, v in value.items()}
        if isinstance(value, (list, tuple)):
            return [self.clean(v) for v in value]
        if isinstance(value, str):
            for secret in sorted(self.values, key=len, reverse=True):
                value = value.replace(secret, '[REDACTED]')
            return re.sub(r'eyJ[\w-]+\.[\w-]+\.[\w-]+', '[REDACTED_JWT]', value)
        return value


def identifier(value, label='ID'):
    if not re.fullmatch(r'[1-9][0-9]*', str(value or '')):
        raise IdaError('INVALID_ID', f'{label} must be a positive integer.')
    return str(value)


def https_url(value):
    u = urllib.parse.urlsplit(value)
    if u.scheme != 'https' or not u.hostname or u.username or u.password or u.fragment:
        raise IdaError('INVALID_URL', 'Use an HTTPS URL without embedded credentials or a fragment.')
    return u


def parse_agent_url(url, *, region=None, agent_id=None, base_url=None, jwt_url=None, a2a_url=None):
    u = https_url(url)
    query = urllib.parse.parse_qs(u.query)
    for key in ('agentId', 'appId', 'region'):
        if len(query.get(key, [])) > 1:
            raise IdaError('AMBIGUOUS_URL', f'The URL contains multiple {key} values.')
    aid = identifier(agent_id or query.get('agentId', [None])[0], 'agentId')
    reg = region or query.get('region', [None])[0]
    if not reg or not re.fullmatch(r'[A-Za-z0-9-]{1,64}', reg):
        raise IdaError('REGION_REQUIRED', 'Supply the region in the page URL or with --region.')
    origin = f'https://{u.netloc}'
    if base_url is None:
        if u.hostname == 'console.volcengine.com' and u.path.startswith('/bi/datawind/'):
            base_url = origin + '/bi/datawind'
        else:
            raise IdaError('BASE_URL_REQUIRED', 'This deployment is not the verified SaaS route; supply its documented --base-url.')
    base_url = base_url.rstrip('/')
    jwt_url = jwt_url or base_url + '/aeolus/api/v3/openapi/jwtToken'
    a2a_url = a2a_url or base_url + '/dataAgent/llm/openApi/v2/a2a/'
    for endpoint in (base_url, jwt_url, a2a_url):
        e = https_url(endpoint)
        if e.query or e.netloc != u.netloc:
            raise IdaError('ENDPOINT_ORIGIN_MISMATCH', 'API endpoints must share the supplied Agent URL origin and have no query string.')
    safe_query = {'agentId': aid, 'region': reg}
    app_id = query.get('appId', [None])[0]
    if app_id:
        safe_query['appId'] = identifier(app_id, 'appId')
    return {'agent_url': origin + u.path + '?' + urllib.parse.urlencode(safe_query),
            'agent_id': aid, 'app_id': app_id, 'region': reg,
            'base_url': base_url, 'jwt_url': jwt_url, 'a2a_url': a2a_url,
            'mode': 'knowledge-qa', 'schema_version': 1}


def fingerprint(profile):
    keys = ('agent_id', 'app_id', 'region', 'base_url', 'jwt_url', 'a2a_url', 'credential_ref')
    return hashlib.sha256(json.dumps({k: profile.get(k) for k in keys}, sort_keys=True).encode()).hexdigest()


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def read_json(response):
    raw = response.read(8 * 1024 * 1024 + 1)
    if len(raw) > 8 * 1024 * 1024:
        raise IdaError('RESPONSE_TOO_LARGE', 'JSON response exceeds 8 MiB.')
    try:
        return json.loads(raw)
    except (ValueError, UnicodeError):
        raise IdaError('INVALID_JSON', 'The API returned a non-JSON response.', http_status=response.status)


def sse_events(response, deadline, idle_timeout=45):
    """Bounded reader; handles UTF-8, multiline data, comments, EOF and stalled sockets."""
    items = queue.Queue(maxsize=32)
    stopped = threading.Event()

    def put(value):
        while not stopped.is_set():
            try:
                items.put(value, timeout=.2)
                return
            except queue.Full:
                pass

    def read():
        try:
            while not stopped.is_set():
                line = response.readline(2 * 1024 * 1024 + 1)
                put(line)
                if not line:
                    return
        except Exception as error:
            put(error)

    threading.Thread(target=read, daemon=True).start()
    last_byte, data, size, total = time.monotonic(), [], 0, 0
    try:
        while True:
            remaining = min(deadline - time.monotonic(), idle_timeout - (time.monotonic() - last_byte))
            if remaining <= 0:
                raise IdaError('STREAM_TIMEOUT', 'Stream deadline or idle timeout reached.')
            try:
                line = items.get(timeout=min(remaining, 1))
            except queue.Empty:
                continue
            if isinstance(line, Exception):
                raise IdaError('STREAM_DISCONNECTED', 'Stream read failed; recover the existing task.')
            last_byte = time.monotonic()
            total += len(line)
            if len(line) > 2 * 1024 * 1024 or total > 64 * 1024 * 1024:
                raise IdaError('STREAM_TOO_LARGE', 'Stream exceeded the configured memory bounds.')
            eof = not line
            try:
                line = line.decode('utf-8-sig').rstrip('\r\n')
            except UnicodeError:
                raise IdaError('INVALID_SSE', 'Invalid UTF-8 in stream.')
            if not line:
                if data:
                    joined = '\n'.join(data)
                    if joined.strip() == '[DONE]':
                        yield {'done': True}
                        return
                    try:
                        yield json.loads(joined)
                    except ValueError:
                        raise IdaError('INVALID_SSE', 'Malformed JSON-RPC event.')
                    data, size = [], 0
                if eof:
                    return
            elif line.startswith('data:'):
                value = line[5:].removeprefix(' ')
                size += len(value)
                if size > 2 * 1024 * 1024:
                    raise IdaError('STREAM_TOO_LARGE', 'SSE event exceeds 2 MiB.')
                data.append(value)
    finally:
        stopped.set()
        # close() may wait for a socket read lock; never block the caller's deadline.
        threading.Thread(target=response.close, daemon=True).start()


class Client:
    def __init__(self, profile, credentials, *, emit=lambda x: None, opener=None, redactor=None):
        self.profile, self.credentials, self.emit = profile, credentials, emit
        self.redactor = redactor or Redactor()
        self.redactor.add(*credentials.values())
        self.opener = opener or urllib.request.build_opener(NoRedirect())
        self.token, self.expires_at = None, 0

    def _http(self, url, body=None, *, auth=True, stream=False, request_id=None, timeout=30):
        rid = request_id or str(uuid.uuid4())
        headers = {'Content-Type': 'application/json', 'x-vedi-region': self.profile['region'],
                   'x-request-id': rid, 'request-id': rid}
        if auth:
            headers['Authorization'] = 'Bearer ' + self.authenticate()
        if stream:
            headers.update({'Accept': 'text/event-stream', 'A2A-Version': '1.0'})
        req = urllib.request.Request(url, data=None if body is None else json.dumps(body, ensure_ascii=False).encode(), headers=headers)
        start = time.monotonic()
        try:
            response = self.opener.open(req, timeout=timeout)
        except urllib.error.HTTPError as error:
            response = error
        except (urllib.error.URLError, TimeoutError, OSError):
            raise IdaError('NETWORK_ERROR', 'Request outcome may be unknown; do not repeat a submission.', request_id=rid)
        self.emit({'event': 'http', 'path': urllib.parse.urlsplit(url).path, 'http_status': response.status,
                   'request_id': rid, 'headers_seconds': round(time.monotonic()-start, 3)})
        return response

    def authenticate(self, force=False):
        if self.token and self.expires_at > time.time() + 60 and not force:
            return self.token
        metadata = {k: v for k, v in self.credentials.items() if k in ('clientId', 'clientSecret', 'proxyUser') and v}
        metadata['expire'] = 3600
        with self._http(self.profile['jwt_url'], {'metadata': metadata}, auth=False) as response:
            if response.status != 200:
                raise IdaError('AUTH_HTTP_ERROR', 'JWT authentication failed.', http_status=response.status)
            payload = read_json(response)
        token = payload.get('data', {}).get('jwtToken')
        if payload.get('code') != 'aeolus/ok' or not token:
            raise IdaError('AUTH_FAILED', 'Client ID, Secret, proxyUser or deployment is not accepted.', api_code=payload.get('code'))
        self.redactor.add(token)
        self.token, self.expires_at = token, time.time() + 3600
        return token

    def call(self, url, body=None, *, stream=False, request_id=None, timeout=30):
        # Only an explicit expired/invalid JWT rejection is replayed. No network/5xx retries.
        for attempt in range(2):
            response = self._http(url, body, stream=stream, request_id=request_id, timeout=timeout)
            if response.status == 401 and attempt == 0:
                response.close()
                self.authenticate(force=True)
                continue
            if response.status != 200:
                status = response.status
                response.close()
                raise IdaError('HTTP_ERROR', 'API request was rejected; verify route, region and permissions.', http_status=status)
            if stream and 'text/event-stream' in response.headers.get('Content-Type', ''):
                return response
            with response:
                payload = read_json(response)
            if payload.get('code') == 'aeolus/openapiClient/tokenExpired' and attempt == 0:
                self.authenticate(force=True)
                continue
            if payload.get('error'):
                error = payload['error']
                raise IdaError('RPC_ERROR', 'IDA returned a JSON-RPC error.', rpc_error=self.redactor.clean(error))
            if 'code' in payload and payload['code'] not in ('llm/ok', 'aeolus/ok'):
                raise IdaError('API_ERROR', 'IDA returned a business error.', api_code=payload['code'], detail=self.redactor.clean(payload.get('msg')))
            if stream:
                raise IdaError('NOT_SSE', 'Stream endpoint returned JSON instead of an SSE stream.')
            return payload
        raise IdaError('AUTH_FAILED', 'JWT refresh did not restore access.')

    def create_session(self, name):
        payload = self.call(self.profile['base_url'] + '/dataAgent/llm/openApi/v2/signed/agent/createSession',
                            {'agentId': int(self.profile['agent_id']), 'name': name[:120], 'source': 'openapi', 'version': 'v5'})
        return identifier(payload.get('data', {}).get('sessionInfo', {}).get('id'), 'sessionId')

    def session_info(self, session_id):
        sid = identifier(session_id, 'sessionId')
        url = self.profile['base_url'] + '/dataAgent/llm/openApi/v2/signed/agent/sessionInfo?'
        payload = self.call(url + urllib.parse.urlencode({'sessionId': sid, 'needMsg': 'true', 'maxMsgCount': 200}))
        data = payload.get('data', {})
        actual = data.get('sessionInfo', {}).get('agentInfo', {}).get('id')
        if str(actual) != self.profile['agent_id']:
            raise IdaError('AGENT_MISMATCH', 'Session does not belong to this profile Agent.')
        return data

    def rpc(self, method, params, *, stream=False, request_id=None, timeout=30):
        rid = request_id or str(uuid.uuid4())
        return self.call(self.profile['a2a_url'], {'jsonrpc': '2.0', 'id': rid, 'method': method, 'params': params},
                         stream=stream, request_id=rid, timeout=timeout)

    def get_task(self, task_id, session_id=None):
        params = {'id': identifier(task_id, 'taskId')}
        if session_id:
            params['contextId'] = identifier(session_id, 'sessionId')
        task = self.rpc('GetTask', params).get('result', {}).get('task', {})
        if str(task.get('id')) != str(task_id) or str(task.get('metadata', {}).get('agentId')) != self.profile['agent_id']:
            raise IdaError('TASK_MISMATCH', 'Task identity or Agent does not match this request.')
        if session_id and str(task.get('contextId')) != str(session_id):
            raise IdaError('SESSION_MISMATCH', 'Task does not belong to the requested session.')
        return task

    def cancel(self, task_id, session_id=None):
        task = self.get_task(task_id, session_id)
        if task.get('status', {}).get('state') in TERMINAL:
            return task
        result = self.rpc('CancelTask', {'id': str(task_id), 'contextId': str(task['contextId'])}).get('result', {})
        if result.get('success') is not True:
            raise IdaError('CANCEL_REJECTED', 'Cancellation was not acknowledged.')
        # Accepted is not canceled. A racing natural completion is also a terminal result.
        for attempt in range(3):
            task = self.get_task(task_id, task['contextId'])
            if task.get('status', {}).get('state') in TERMINAL:
                return task
            if attempt < 2:
                time.sleep(.5)
        raise IdaError('CANCEL_PENDING', 'Cancellation accepted but not yet confirmed.', task_id=str(task_id))

    def send(self, session_id, message, *, mode='knowledge-qa', request_id=None, timeout=30):
        if not isinstance(message, str) or not message.strip() or len(message) > 100000:
            raise IdaError('INVALID_MESSAGE', 'Provide 1–100000 characters.')
        metadata = {'sessionId': identifier(session_id), 'version': 'v5', 'enableKnowledgeQA': mode == 'knowledge-qa'}
        # Mode is not a tool permission boundary. Respect the Agent's configured tools.
        if mode == 'knowledge-qa':
            metadata['enableFastMode'] = True
        return self.rpc('SendStreamingMessage', {'message': {'role': 'ROLE_USER', 'parts': [{'text': message}], 'metadata': metadata}},
                        stream=True, request_id=request_id, timeout=timeout)

    def result(self, task_id, session_id=None):
        task = self.get_task(task_id, session_id)
        data = self.session_info(task['contextId'])
        return assess_result(task, data)


def assess_result(task, data):
    """Correlate the exact task with its persisted final answer, not neighboring messages."""
    task_id = str(task['id'])
    answers, reports, conflicts = [], [], []
    for item in data.get('messageList', []):
        content = item.get('content', {})
        if not isinstance(content, dict):
            continue
        inner = content.get('content', {})
        if not isinstance(inner, dict):
            continue
        final = inner.get('deepResearchContent', {})
        if not isinstance(final, dict) or str(final.get('taskId')) != task_id:
            continue
        value = final.get('finalAnswer', [])
        if isinstance(value, str):
            value = [value]
        if isinstance(value, list):
            answers.extend(x for x in value if isinstance(x, str) and x.strip())
        values = final.get('finalReport', [])
        if isinstance(values, list):
            reports.extend(x for x in values if x)
    state = task.get('status', {}).get('state', 'UNKNOWN')
    for history in data.get('tasks', []):
        if str(history.get('task', {}).get('id')) != task_id:
            continue
        for event in history.get('events', []):
            reconstructed = event.get('statusUpdate', {}).get('status', {}).get('state')
            if reconstructed in TERMINAL and reconstructed != state:
                conflicts.append({'snapshot': state, 'reconstructed': reconstructed})
    if state == 'TASK_STATE_COMPLETED' and answers:
        verdict = 'SUCCESS'
    elif state == 'TASK_STATE_COMPLETED' and reports:
        verdict = 'ARTIFACT_REVIEW_REQUIRED'
    elif state == 'TASK_STATE_COMPLETED':
        verdict = 'COMPLETED_NO_FINAL_ANSWER'
    elif state in TERMINAL:
        verdict = state.removeprefix('TASK_STATE_')
    elif state in ACTIVE:
        verdict = 'INPUT_REQUIRED' if state == 'TASK_STATE_INPUT_REQUIRED' else 'IN_PROGRESS'
    else:
        verdict = 'UNKNOWN'
    return {'verdict': verdict, 'task_id': task_id, 'session_id': str(task['contextId']),
            'state': state, 'answer': '\n'.join(dict.fromkeys(answers)), 'reports': reports,
            'conflicts': conflicts, 'final_answer_persisted': bool(answers),
            'artifact_validation': 'persisted_references_only' if reports else None,
            'ui_verified': False}
