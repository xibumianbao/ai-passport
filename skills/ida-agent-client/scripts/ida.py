#!/usr/bin/env python3
"""Reusable IDA Skill CLI. Run --help; outputs redacted NDJSON."""
from __future__ import annotations

import argparse
import datetime as dt
import getpass
import hashlib
import json
import os
from pathlib import Path
import re
import sys
import time
import uuid

from ida_core import (ACTIVE, TERMINAL, DOCS, DOC_ROOT, VERSION, Client, IdaError,
                      Redactor, fingerprint, identifier, parse_agent_url, sse_events)
from ida_credentials import CredentialStore


def now():
    return dt.datetime.now(dt.timezone.utc).isoformat()


def default_home():
    if os.name == 'nt':
        base = Path(os.environ.get('LOCALAPPDATA', Path.home() / 'AppData' / 'Local'))
    elif sys.platform == 'darwin':
        base = Path.home() / 'Library' / 'Application Support'
    else:
        base = Path(os.environ.get('XDG_DATA_HOME', Path.home() / '.local' / 'share'))
    return base / 'ida-agent-client'


def safe_name(value):
    if not re.fullmatch(r'[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}', value):
        raise IdaError('INVALID_PROFILE', 'Profile names use 1–64 letters, digits, underscores or hyphens.')
    return value


def write_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    temp = path.with_name(path.name + '.' + uuid.uuid4().hex + '.tmp')
    try:
        with open(temp, 'x', encoding='utf-8') as handle:
            os.chmod(temp, 0o600)
            json.dump(value, handle, ensure_ascii=False, indent=2)
        os.replace(temp, path)
    finally:
        if temp.exists():
            temp.unlink()


class LocalState:
    def __init__(self, home, name):
        self.home, self.name = Path(home).expanduser().resolve(), safe_name(name)
        self.profile_path = self.home / 'profiles' / (self.name + '.json')

    def load(self):
        if not self.profile_path.exists():
            raise IdaError('INITIALIZATION_REQUIRED', 'Run init and supply the full Agent page URL, Client ID and Secret.', profile=self.name)
        p = json.loads(self.profile_path.read_text(encoding='utf-8'))
        if p.get('schema_version') != 1:
            raise IdaError('CONFIG_VERSION', 'Unsupported profile version.')
        normalized = parse_agent_url(p['agent_url'], base_url=p['base_url'], jwt_url=p['jwt_url'], a2a_url=p['a2a_url'])
        if any(p.get(k) != normalized[k] for k in ('agent_id', 'app_id', 'region')):
            raise IdaError('INVALID_CONFIG', 'Profile identity differs from its Agent URL; initialize it again.')
        if not re.fullmatch(r'[0-9a-f-]{36}', p.get('credential_ref', '')):
            raise IdaError('INVALID_CONFIG', 'Invalid credential reference.')
        return p

    def save_profile(self, p):
        write_json(self.profile_path, p)

    def run_path(self, run_id):
        if not re.fullmatch(r'[0-9a-f-]{36}', run_id):
            raise IdaError('INVALID_RUN', 'Invalid run ID.')
        return self.home / 'runs' / self.name / (run_id + '.json')

    def new_run(self, profile, **fields):
        run = {'run_id': str(uuid.uuid4()), 'profile': self.name, 'fingerprint': fingerprint(profile),
               'created_at': now(), 'stage': 'PREPARED', **fields}
        self.save_run(run)
        return run

    def save_run(self, run):
        run['updated_at'] = now()
        write_json(self.run_path(run['run_id']), run)

    def load_run(self, run_id, profile):
        p = self.run_path(run_id)
        if not p.exists():
            raise IdaError('RUN_NOT_FOUND', 'No such local run in this profile.')
        run = json.loads(p.read_text(encoding='utf-8'))
        if run.get('fingerprint') != fingerprint(profile):
            raise IdaError('PROFILE_CHANGED', 'This run belongs to a different Agent, deployment or credential profile.')
        return run


class Output:
    def __init__(self, redactor, path=None, event_path=None):
        self.redactor, self.path, self.event_path = redactor, path, event_path
        self.events_handle = None
        if event_path:
            # Explicit capture only; never silently replace a previous trace.
            self.events_handle = open(event_path, 'x', encoding='utf-8')
            os.chmod(event_path, 0o600)

    def emit(self, value):
        print(json.dumps(self.redactor.clean(value), ensure_ascii=False), flush=True)

    def capture(self, value):
        if self.events_handle:
            self.events_handle.write(json.dumps(self.redactor.clean(value), ensure_ascii=False) + '\n')
            self.events_handle.flush()

    def final(self, result):
        value = self.redactor.clean(result)
        if self.path:
            write_json(Path(self.path), value)
        self.emit({'event': 'result', **value})


def reconcile(client, task_id, session_id):
    result = client.result(task_id, session_id)
    # Brief read-after-write allowance. Never re-submit the question.
    for _ in range(2):
        if result['verdict'] != 'COMPLETED_NO_FINAL_ANSWER':
            break
        time.sleep(.5)
        result = client.result(task_id, session_id)
    return result


def follow(client, local, run, output, *, response=None, seconds=300, idle_timeout=45,
           reconnects=1, cancel_on_timeout=False, show_process=False):
    start, errors, completed_event, attempts = time.monotonic(), [], False, 0
    deadline = start + seconds
    artifact_text = {}
    try:
        while time.monotonic() < deadline:
            if response is None:
                if not run.get('task_id'):
                    raise IdaError('SUBMISSION_UNKNOWN', 'No task ID was received; inspect session history before considering another submission.', run_id=run['run_id'], session_id=run.get('session_id'))
                task = client.get_task(run['task_id'], run.get('session_id'))
                if task.get('status', {}).get('state') in TERMINAL or task.get('status', {}).get('state') == 'TASK_STATE_INPUT_REQUIRED':
                    break
                response = client.rpc('SubscribeToTask', {'id': run['task_id'], 'contextId': run['session_id']},
                                      stream=True, timeout=max(.1, min(30, deadline-time.monotonic())))
                artifact_text = {}  # Subscription replay is a fresh snapshot, not additional answer text.
            terminal = False
            try:
                for envelope in sse_events(response, deadline, idle_timeout):
                    output.capture({'elapsed_seconds': round(time.monotonic()-start, 3), 'envelope': envelope})
                    if envelope.get('done'):
                        break
                    if envelope.get('error'):
                        raise IdaError('RPC_ERROR', 'Stream returned a JSON-RPC error.', rpc_error=envelope['error'])
                    result = envelope.get('result', {})
                    task = result.get('task', {})
                    agent = task.get('metadata', {}).get('agentId')
                    if agent is not None and str(agent) != client.profile['agent_id']:
                        raise IdaError('AGENT_MISMATCH', 'SSE task belongs to another Agent.')
                    status = result.get('statusUpdate', {})
                    update = result.get('artifactUpdate', {})
                    tid = task.get('id') or status.get('taskId') or update.get('taskId')
                    context = task.get('contextId') or status.get('contextId') or update.get('contextId')
                    if context is not None and str(context) != run['session_id']:
                        raise IdaError('SESSION_MISMATCH', 'SSE event belongs to another session.')
                    if tid:
                        tid = identifier(tid, 'taskId')
                        if run.get('task_id') and tid != run['task_id']:
                            raise IdaError('TASK_MISMATCH', 'SSE event belongs to another task.')
                        if not run.get('task_id'):
                            run.update(task_id=tid, stage='RUNNING')
                            local.save_run(run)
                            output.emit({'event': 'task', 'task_id': tid, 'session_id': run['session_id'], 'run_id': run['run_id']})
                    if update:
                        artifact = update.get('artifact', {})
                        aid = str(artifact.get('artifactId', 'unknown'))
                        text = ''.join(x.get('text', '') for x in artifact.get('parts', []) if isinstance(x.get('text'), str))
                        artifact_text[aid] = artifact_text.get(aid, '') + text if update.get('append') else text
                        if show_process and text:
                            output.emit({'event': 'process_text', 'is_final_answer': False, 'artifact_id': aid,
                                         'append': bool(update.get('append')), 'text': text})
                    state = status.get('status', {}).get('state') or task.get('status', {}).get('state')
                    if state and state != run.get('last_stream_state'):
                        run['last_stream_state'] = state
                        local.save_run(run)
                        output.emit({'event': 'task_state', 'state': state, 'elapsed_seconds': round(time.monotonic()-start, 3)})
                    if state in TERMINAL or state == 'TASK_STATE_INPUT_REQUIRED':
                        completed_event = state == 'TASK_STATE_COMPLETED' and status.get('final') is True
                        terminal = True
                        break
            except IdaError as error:
                if error.code in ('TASK_MISMATCH', 'SESSION_MISMATCH', 'AGENT_MISMATCH', 'RPC_ERROR', 'INVALID_SSE', 'STREAM_TOO_LARGE'):
                    raise
                errors.append(error.code)
                output.emit({'event': 'stream_interrupted', 'code': error.code, 'task_id': run.get('task_id')})
            finally:
                response = None  # Reader owns response cleanup.
            if terminal or attempts >= reconnects or time.monotonic() >= deadline:
                break
            if not run.get('task_id'):
                break
            attempts += 1
            output.emit({'event': 'reconnecting', 'task_id': run['task_id'], 'attempt': attempts})
        if not run.get('task_id'):
            run['stage'] = 'SUBMISSION_UNKNOWN'
            raise IdaError('SUBMISSION_UNKNOWN', 'Submission may have been accepted. No automatic re-send is safe.', run_id=run['run_id'], session_id=run.get('session_id'))
        result = reconcile(client, run['task_id'], run['session_id'])
        if cancel_on_timeout and result['verdict'] == 'IN_PROGRESS':
            client.cancel(run['task_id'], run['session_id'])
            result = client.result(run['task_id'], run['session_id'])
            result['canceled_by_client'] = True
        run.update(stage=result['verdict'], task_state=result['state'])
        local.save_run(run)
        return {**result, 'run_id': run['run_id'], 'stream_completed_event': completed_event,
                'elapsed_seconds': round(time.monotonic()-start, 3), 'reconnections': attempts,
                'stream_errors': errors, 'resume_command': f'--profile {local.name} resume --run-id {run["run_id"]}'}
    except BaseException:
        if run.get('task_id'):
            run['stage'] = 'INTERRUPTED_CHECK_STATUS'
        elif run.get('session_id'):
            run['stage'] = 'SUBMISSION_UNKNOWN'
        local.save_run(run)
        raise


def chat(client, local, profile, output, message, *, session_id=None, name=None, mode='knowledge-qa', **options):
    if not message.strip() or len(message) > 100000:
        raise IdaError('INVALID_MESSAGE', 'Provide 1–100000 characters.')
    if session_id:
        client.session_info(session_id)  # Verify Agent ownership before submitting.
    run = local.new_run(profile, session_id=str(session_id) if session_id else None,
                        request_id=str(uuid.uuid4()), message_sha256=hashlib.sha256(message.encode()).hexdigest(), mode=mode)
    output.emit({'event': 'run_prepared', 'run_id': run['run_id']})
    if not session_id:
        run['stage'] = 'CREATING_SESSION'
        local.save_run(run)
        try:
            run['session_id'] = client.create_session(name or ('IDA ' + now()))
        except BaseException:
            run['stage'] = 'SESSION_CREATE_UNCONFIRMED'
            local.save_run(run)
            raise
    run['stage'] = 'SUBMITTING'
    local.save_run(run)
    output.emit({'event': 'session', 'session_id': run['session_id'], 'run_id': run['run_id']})
    try:
        response = client.send(run['session_id'], message, mode=mode, request_id=run['request_id'], timeout=min(30, options.get('seconds', 300)))
    except BaseException:
        run['stage'] = 'SUBMISSION_UNCONFIRMED'
        local.save_run(run)
        raise
    return follow(client, local, run, output, response=response, **options)


def credentials_input(json_prompt=False):
    # getpass must not silently fall back to echoed input on a non-interactive console.
    if not sys.stdin.isatty():
        raise IdaError('INTERACTIVE_INPUT_REQUIRED', 'Use an interactive hidden prompt or --credential-store env.')
    import warnings
    with warnings.catch_warnings():
        warnings.simplefilter('error', getpass.GetPassWarning)
        if json_prompt:
            value = json.loads(getpass.getpass('Credentials JSON (hidden): '))
        else:
            value = {'clientId': getpass.getpass('Client ID (hidden): '),
                     'clientSecret': getpass.getpass('Secret (hidden): ')}
        if set(value) - {'clientId', 'clientSecret', 'proxyUser'}:
            raise IdaError('INVALID_CREDENTIALS', 'Credential input supports clientId, clientSecret and optional proxyUser only.')
        if not all(isinstance(value.get(k), str) and value[k].strip() for k in ('clientId', 'clientSecret')):
            raise IdaError('INVALID_CREDENTIALS', 'Client ID and Secret are required.')
        return value


def parser():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--version', action='version', version=VERSION)
    p.add_argument('--profile', default='default')
    p.add_argument('--data-dir', type=Path, default=default_home())
    p.add_argument('--output', help='Explicit final JSON report path (may contain the user answer).')
    p.add_argument('--events-output', help='Explicit redacted raw SSE capture; must be a new file.')
    sub = p.add_subparsers(dest='command', required=True)
    init = sub.add_parser('init', help='First-use wizard: URL + credentials + live end-to-end smoke test.')
    for arg in ('agent-url', 'base-url', 'jwt-url', 'a2a-url', 'region', 'agent-id'):
        init.add_argument('--' + arg)
    init.add_argument('--credential-store', choices=('system', 'env'), default='system')
    init.add_argument('--credentials-prompt-json', action='store_true')
    init.add_argument('--replace', action='store_true', help='Replace only this named profile after authentication succeeds.')
    init.add_argument('--skip-smoke', action='store_true', help='Save authenticated configuration without claiming chat verification.')
    sub.add_parser('profiles', help='List local profile names, never credentials.')
    sub.add_parser('show', help='Show the selected profile without credentials.')
    sub.add_parser('auth', help='Verify/refresh JWT in memory; never print the JWT.')
    sub.add_parser('doctor', help='Check configuration and authentication without creating a task.')
    sc = sub.add_parser('session-create')
    sc.add_argument('--name', default='IDA session')
    si = sub.add_parser('session-info')
    si.add_argument('--session-id', required=True)
    ch = sub.add_parser('chat')
    msg = ch.add_mutually_exclusive_group(required=True)
    msg.add_argument('--message')
    msg.add_argument('--message-file', type=Path)
    ch.add_argument('--session-id', help='Explicitly continue this session; otherwise create a new session.')
    ch.add_argument('--name')
    ch.add_argument('--mode', choices=('knowledge-qa', 'deep-research'), default='knowledge-qa')
    for cmd in ('status', 'result', 'subscribe', 'cancel'):
        s = sub.add_parser(cmd)
        s.add_argument('--task-id', required=True)
        s.add_argument('--session-id')
    rs = sub.add_parser('resume')
    rs.add_argument('--run-id', required=True)
    for s in (ch, rs, sub.choices['subscribe']):
        s.add_argument('--deadline', type=float, default=300, help='Observation deadline in seconds; does not cancel by default.')
        s.add_argument('--idle-timeout', type=float, default=45)
        s.add_argument('--max-reconnects', type=int, choices=range(4), default=1)
        s.add_argument('--cancel-on-timeout', action='store_true')
        s.add_argument('--show-process-text', action='store_true', help='Show explicitly labeled process text, not final replies.')
    ds = sub.add_parser('docs')
    ds.add_argument('--format', choices=('json', 'markdown'), default='markdown')
    return p


def execute(args, output, redactor):
    local = LocalState(args.data_dir, args.profile)
    if args.command == 'profiles':
        return {'profiles': sorted(x.stem for x in (local.home / 'profiles').glob('*.json'))}
    if args.command == 'docs':
        docs = {key: DOC_ROOT + value + '?lang=zh' for key, value in DOCS.items()}
        endpoints = {}
        if local.profile_path.exists():
            profile = local.load()
            endpoints = {k: profile[k] for k in ('base_url', 'jwt_url', 'a2a_url')}
        if args.format == 'markdown':
            print('\n'.join(f'- [{key}]({url})' for key, url in docs.items()), flush=True)
        return {'documentation': docs, 'resolved_endpoints': endpoints, 'verified_protocol': 'IDA v2 A2A', 'version': VERSION}
    if args.command == 'init':
        if local.profile_path.exists() and not args.replace:
            raise IdaError('PROFILE_EXISTS', 'This profile is already initialized. Use show/doctor or explicitly --replace.')
        url = args.agent_url or input('Full Agent page URL: ').strip()
        profile = parse_agent_url(url, region=args.region, agent_id=args.agent_id,
                                  base_url=args.base_url, jwt_url=args.jwt_url, a2a_url=args.a2a_url)
        store = CredentialStore(args.credential_store)
        credentials = store.get('environment') if args.credential_store == 'env' else credentials_input(args.credentials_prompt_json)
        redactor.add(*credentials.values())
        profile.update(name=local.name, credential_store=args.credential_store, credential_ref=str(uuid.uuid4()), created_at=now())
        client = Client(profile, credentials, emit=output.emit, redactor=redactor)
        client.authenticate()
        old = local.load() if local.profile_path.exists() else None
        store.put(profile['credential_ref'], credentials)
        profile['initialization'] = 'AUTHENTICATED_CHAT_UNVERIFIED'
        try:
            local.save_profile(profile)
        except Exception:
            store.delete(profile['credential_ref'])
            raise
        if old:
            CredentialStore(old['credential_store']).delete(old['credential_ref'])
        output.emit({'event': 'profile_saved', 'profile': local.name, 'agent_id': profile['agent_id'], 'credential_store': args.credential_store})
        if args.skip_smoke:
            return {'verdict': 'AUTHENTICATED_CHAT_UNVERIFIED', 'profile': local.name}
        result = chat(client, local, profile, output, '1+1 等于几？只回复数字答案，然后结束本轮任务。',
                      name='IDA Skill initialization ' + now(), seconds=90, cancel_on_timeout=True)
        if result['verdict'] == 'SUCCESS' and result['answer'].strip().rstrip('。.!') == '2':
            profile['initialization'] = 'VERIFIED'
            profile['verified_at'] = now()
        else:
            profile['initialization'] = 'SMOKE_NOT_PASSED'
            result['smoke_passed'] = False
        local.save_profile(profile)
        return {**result, 'profile': local.name, 'initialization': profile['initialization']}
    profile = local.load()
    if args.command == 'show':
        return {'profile': profile}
    credentials = CredentialStore(profile['credential_store']).get(profile['credential_ref'])
    redactor.add(*credentials.values())
    client = Client(profile, credentials, emit=output.emit, redactor=redactor)
    if args.command in ('auth', 'doctor'):
        client.authenticate(force=True)
        return {'verdict': 'AUTHENTICATED', 'profile': local.name, 'agent_id': profile['agent_id'],
                'token_printed': False, 'token_storage': 'process_memory', 'initialization': profile.get('initialization'),
                'region': profile['region'], 'version': VERSION}
    if args.command == 'session-create':
        return {'session_id': client.create_session(args.name), 'agent_id': profile['agent_id']}
    if args.command == 'session-info':
        data = client.session_info(args.session_id)
        # Project only session history, not credentials or the Agent's full configuration.
        return {'session_id': str(args.session_id), 'session_name': data.get('sessionInfo', {}).get('name'),
                'latest_task_status': data.get('sessionInfo', {}).get('latestTaskStatus'),
                'messages': data.get('messageList', []), 'tasks': data.get('tasks', [])}
    if args.command == 'status':
        task = client.get_task(args.task_id, args.session_id)
        return {'task_id': str(task['id']), 'session_id': str(task['contextId']), 'state': task.get('status', {}).get('state'),
                'metadata': task.get('metadata'), 'result_not_checked': True}
    if args.command == 'result':
        return reconcile(client, args.task_id, args.session_id)
    if args.command == 'cancel':
        task = client.cancel(args.task_id, args.session_id)
        return {'task_id': str(task['id']), 'state': task.get('status', {}).get('state'), 'terminal_confirmed': task.get('status', {}).get('state') in TERMINAL}
    if not 0 < args.deadline <= 86400 or not 0 < args.idle_timeout <= 3600:
        raise IdaError('INVALID_TIMEOUT', 'Use deadline 0–86400 seconds and idle timeout 0–3600 seconds, both positive.')
    options = dict(seconds=args.deadline, idle_timeout=args.idle_timeout, reconnects=args.max_reconnects,
                   cancel_on_timeout=args.cancel_on_timeout, show_process=args.show_process_text)
    if args.command == 'chat':
        message = args.message if args.message is not None else args.message_file.read_text(encoding='utf-8')
        return chat(client, local, profile, output, message, session_id=args.session_id, name=args.name, mode=args.mode, **options)
    if args.command == 'resume':
        run = local.load_run(args.run_id, profile)
        if not run.get('task_id'):
            raise IdaError('SUBMISSION_UNKNOWN', 'This run has no task ID. Read its session history; do not automatically send again.', session_id=run.get('session_id'))
    else:  # subscribe to an explicitly identified existing task; never send a new message.
        task = client.get_task(args.task_id, args.session_id)
        run = local.new_run(profile, session_id=str(task['contextId']), task_id=str(task['id']), stage='SUBSCRIBING')
        response = client.rpc('SubscribeToTask', {'id': run['task_id'], 'contextId': run['session_id']},
                              stream=True, timeout=min(30, args.deadline))
        return follow(client, local, run, output, response=response, **options)
    return follow(client, local, run, output, **options)


def main(argv=None):
    args = parser().parse_args(argv)
    redactor = Redactor()
    output = None
    try:
        output = Output(redactor, args.output, args.events_output)
        result = execute(args, output, redactor)
        output.final(result)
        return 0 if result.get('verdict', 'SUCCESS') in ('SUCCESS', 'AUTHENTICATED') and result.get('smoke_passed') is not False else 2
    except KeyboardInterrupt:
        if output:
            output.emit({'event': 'error', 'code': 'INTERRUPTED', 'message': 'Local observation stopped. The cloud task may still be running; use status/resume/cancel.'})
        return 130
    except IdaError as error:
        value = {'event': 'error', 'code': error.code, 'message': str(error), **error.details}
        (output.emit if output else print)(redactor.clean(value))
        return 1
    except Exception as error:
        # No tracebacks, credential-bearing response bodies or raw OS errors in normal output.
        value = {'event': 'error', 'code': 'LOCAL_ERROR', 'type': type(error).__name__, 'message': 'Local operation failed; check input, file permissions and the selected credential store.'}
        (output.emit if output else print)(value)
        return 1
    finally:
        if output and output.events_handle:
            output.events_handle.close()


if __name__ == '__main__':
    sys.exit(main())
