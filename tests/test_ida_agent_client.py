"""Behavioral tests: all identifiers and response data here are synthetic."""
import io
import json
import os
from pathlib import Path
import sys
import tempfile
import time
import unittest
from unittest.mock import patch
import urllib.error

SCRIPTS = Path(__file__).resolve().parents[1] / 'skills' / 'ida-agent-client' / 'scripts'
sys.path.insert(0, str(SCRIPTS))
import ida
from ida_core import Client, IdaError, Redactor, assess_result, fingerprint, parse_agent_url, sse_events

URL = 'https://console.volcengine.com/bi/datawind/data-agent/analytics-agent/pages/agent/home?agentId=101&appId=202&region=cn-shanghai'


def profile():
    p = parse_agent_url(URL)
    p.update(credential_ref='00000000-0000-0000-0000-000000000001', credential_store='env')
    return p


def frame(result):
    return 'data: ' + json.dumps({'jsonrpc': '2.0', 'result': result}, ensure_ascii=False) + '\n\n'


def task(state='TASK_STATE_COMPLETED'):
    return {'id': '303', 'contextId': '404', 'status': {'state': state}, 'metadata': {'agentId': 101}}


def final_data(answer='2', task_id=303):
    return {'sessionInfo': {'agentInfo': {'id': 101}}, 'messageList': [
        {'content': {'content': {'deepResearchContent': {'taskId': task_id, 'finalAnswer': [answer]}}}}]}


class Response(io.BytesIO):
    def __init__(self, body, status=200, content_type='application/json'):
        if not isinstance(body, (str, bytes)):
            body = json.dumps(body)
        super().__init__(body.encode() if isinstance(body, str) else body)
        self.status, self.headers = status, {'Content-Type': content_type}


class Opener:
    def __init__(self, responses):
        self.responses, self.requests = list(responses), []

    def open(self, request, timeout=30):
        self.requests.append(request)
        result = self.responses.pop(0)
        if isinstance(result, Exception):
            raise result
        return result


class ConfigAndSecurity(unittest.TestCase):
    def test_url_parses_and_removes_unrelated_query_values(self):
        p = parse_agent_url(URL + '&token=should-not-persist')
        self.assertEqual(p['agent_id'], '101')
        self.assertEqual(p['base_url'], 'https://console.volcengine.com/bi/datawind')
        self.assertNotIn('token', p['agent_url'])

    def test_non_saas_requires_explicit_documented_base(self):
        url = URL.replace('console.volcengine.com', 'ida.example.org')
        with self.assertRaisesRegex(IdaError, 'documented'):
            parse_agent_url(url)
        p = parse_agent_url(url, base_url='https://ida.example.org/bi')
        self.assertTrue(p['a2a_url'].startswith('https://ida.example.org/bi/'))

    def test_rejects_ambiguous_id_or_cross_origin_credentials(self):
        for value, options in ((URL + '&agentId=999', {}), (URL, {'jwt_url': 'https://other.example.org/token'}),
                               (URL.replace('https:', 'http:'), {})):
            with self.assertRaises(IdaError):
                parse_agent_url(value, **options)

    def test_redacts_nested_credentials_and_echoed_values(self):
        redactor = Redactor()
        redactor.add('demo-credential-value')
        value = redactor.clean({'clientId': 'some-id', 'nested': ['oops demo-credential-value'], 'Authorization': 'Bearer abc'})
        self.assertNotIn('some-id', json.dumps(value))
        self.assertNotIn('demo-credential-value', json.dumps(value))
        self.assertNotIn('Bearer abc', json.dumps(value))

    def test_journals_do_not_contain_credentials_and_profile_change_blocks_resume(self):
        with tempfile.TemporaryDirectory() as tmp:
            local = ida.LocalState(tmp, 'demo')
            p = profile()
            run = local.new_run(p, session_id='404', task_id='303')
            self.assertEqual(local.load_run(run['run_id'], p)['task_id'], '303')
            p['agent_id'] = '999'
            with self.assertRaises(IdaError):
                local.load_run(run['run_id'], p)
            self.assertNotIn('clientSecret', local.run_path(run['run_id']).read_text())

    def test_path_traversal_profile_rejected(self):
        with self.assertRaises(IdaError):
            ida.LocalState('.', '../another-profile')

    def test_profile_url_identity_cannot_be_changed_independently(self):
        with tempfile.TemporaryDirectory() as tmp:
            local = ida.LocalState(tmp, 'demo')
            p = profile()
            p['agent_id'] = '999'
            local.save_profile(p)
            with self.assertRaisesRegex(IdaError, 'identity differs'):
                local.load()


class ResultAcceptance(unittest.TestCase):
    def test_completed_and_persisted_final_answer_passes(self):
        data = final_data()
        data['tasks'] = [{'task': {'id': '303'}, 'summary': {'hasFinalMessage': False, 'hasFinalArtifact': False}}]
        self.assertEqual(assess_result(task(), data)['verdict'], 'SUCCESS')

    def test_process_text_is_not_final_answer(self):
        t = task()
        t['artifacts'] = [{'parts': [{'text': '2'}], 'metadata': {'stepType': 'executor_think'}}]
        self.assertEqual(assess_result(t, {})['verdict'], 'COMPLETED_NO_FINAL_ANSWER')

    def test_never_reuses_another_turn_answer(self):
        self.assertEqual(assess_result(task(), final_data(task_id=999))['verdict'], 'COMPLETED_NO_FINAL_ANSWER')

    def test_canceled_snapshot_wins_over_synthetic_completed_event(self):
        data = final_data()
        data['tasks'] = [{'task': {'id': '303'}, 'events': [{'statusUpdate': {'status': {'state': 'TASK_STATE_COMPLETED'}, 'final': True}}]}]
        result = assess_result(task('TASK_STATE_CANCELED'), data)
        self.assertEqual(result['verdict'], 'CANCELED')
        self.assertEqual(len(result['conflicts']), 1)

    def test_report_reference_requires_review_not_claimed_download_verification(self):
        data = final_data('')
        data['messageList'][0]['content']['content']['deepResearchContent']['finalReport'] = [{'url': 'https://example.org/report'}]
        self.assertEqual(assess_result(task(), data)['verdict'], 'ARTIFACT_REVIEW_REQUIRED')


class Transport(unittest.TestCase):
    def test_sse_unicode_multiline_comments_and_done(self):
        stream = Response(': keepalive\r\ndata: {"text":\r\ndata: "中文"}\r\n\r\ndata: [DONE]\n\n')
        values = list(sse_events(stream, time.monotonic()+2))
        self.assertEqual(values, [{'text': '中文'}, {'done': True}])

    def test_sse_flushes_last_event_without_trailing_blank_line(self):
        values = list(sse_events(Response('data: {"ok": true}'), time.monotonic()+2))
        self.assertEqual(values, [{'ok': True}])

    def test_bad_sse_is_not_silently_ignored(self):
        with self.assertRaises(IdaError):
            list(sse_events(Response('data: broken\n\n'), time.monotonic()+2))

    def test_idle_stream_does_not_hold_caller_past_deadline(self):
        class Slow(Response):
            def readline(self, *args):
                time.sleep(.2)
                return b''
        start = time.monotonic()
        with self.assertRaises(IdaError):
            list(sse_events(Slow(''), start+.04, idle_timeout=1))
        self.assertLess(time.monotonic()-start, .18)

    def test_explicit_auth_rejection_refreshes_once(self):
        opener = Opener([Response({'code': 'aeolus/ok', 'data': {'jwtToken': 'fake-token-1'}}),
                         Response({}, 401), Response({'code': 'aeolus/ok', 'data': {'jwtToken': 'fake-token-2'}}),
                         Response({'code': 'llm/ok', 'data': {}})])
        client = Client(profile(), {'clientId': 'fake', 'clientSecret': 'fake-secret'}, opener=opener)
        client.call(profile()['base_url']+'/example', {'action': 'example'})
        self.assertEqual(len(opener.requests), 4)
        self.assertEqual(opener.requests[-1].headers['Authorization'], 'Bearer fake-token-2')

    def test_network_error_does_not_repeat_submission(self):
        opener = Opener([urllib.error.URLError('network closed')])
        client = Client(profile(), {}, opener=opener)
        client.token, client.expires_at = 'fake-token', time.time()+3600
        with self.assertRaises(IdaError):
            client.send('404', 'hello')
        self.assertEqual(len(opener.requests), 1)

    def test_agent_mismatch_is_rejected(self):
        opener = Opener([Response({'code': 'llm/ok', 'data': {'sessionInfo': {'agentInfo': {'id': 999}}}})])
        client = Client(profile(), {}, opener=opener)
        client.token, client.expires_at = 'fake-token', time.time()+3600
        with self.assertRaises(IdaError):
            client.session_info('404')

    def test_cancellation_acceptance_is_followed_by_readback(self):
        opener = Opener([Response({'result': {'task': task('TASK_STATE_WORKING')}}),
                         Response({'result': {'success': True}}),
                         Response({'result': {'task': task('TASK_STATE_CANCELED')}})])
        client = Client(profile(), {}, opener=opener)
        client.token, client.expires_at = 'fake-token', time.time()+3600
        self.assertEqual(client.cancel('303')['status']['state'], 'TASK_STATE_CANCELED')
        self.assertEqual(len(opener.requests), 3)


class FakeClient:
    def __init__(self, no_task=False, remains_active=False):
        self.profile = profile()
        self.creates = self.sends = self.subscribes = self.cancels = 0
        self.no_task, self.remains_active = no_task, remains_active

    def create_session(self, name):
        self.creates += 1
        return '404'

    def send(self, session_id, message, **kwargs):
        self.sends += 1
        return Response('' if self.no_task else frame({'task': task('TASK_STATE_WORKING')}))

    def get_task(self, *args):
        return task('TASK_STATE_CANCELED' if self.cancels else 'TASK_STATE_WORKING')

    def rpc(self, *args, **kwargs):
        self.subscribes += 1
        return Response(frame({'statusUpdate': {'taskId': '303', 'contextId': '404', 'status': {'state': 'TASK_STATE_COMPLETED'}, 'final': True}}))

    def result(self, *args):
        state = 'TASK_STATE_CANCELED' if self.cancels else 'TASK_STATE_WORKING' if self.remains_active else 'TASK_STATE_COMPLETED'
        return assess_result(task(state), final_data())

    def cancel(self, *args):
        self.cancels += 1
        return task('TASK_STATE_CANCELED')


class Recovery(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.local = ida.LocalState(self.tmp.name, 'demo')
        self.output = ida.Output(Redactor())
        self.output.emit = lambda x: None

    def tearDown(self):
        self.tmp.cleanup()

    def test_disconnect_recovers_same_task_and_never_resends_question(self):
        client = FakeClient()
        result = ida.chat(client, self.local, profile(), self.output, 'synthetic question', seconds=2)
        self.assertEqual(result['verdict'], 'SUCCESS')
        self.assertEqual((client.creates, client.sends, client.subscribes), (1, 1, 1))
        record = self.local.load_run(result['run_id'], profile())
        self.assertEqual(record['task_id'], '303')
        self.assertNotIn('synthetic question', self.local.run_path(result['run_id']).read_text())

    def test_no_task_id_is_unknown_and_never_automatically_retried(self):
        client = FakeClient(no_task=True)
        with self.assertRaises(IdaError) as error:
            ida.chat(client, self.local, profile(), self.output, 'hello', seconds=2)
        self.assertEqual(error.exception.code, 'SUBMISSION_UNKNOWN')
        self.assertEqual(client.sends, 1)
        self.assertEqual(client.subscribes, 0)

    def test_observation_end_does_not_cancel_by_default(self):
        client = FakeClient(remains_active=True)
        result = ida.chat(client, self.local, profile(), self.output, 'hello', seconds=2, reconnects=0)
        self.assertEqual(result['verdict'], 'IN_PROGRESS')
        self.assertEqual(client.cancels, 0)

    def test_explicit_cancel_policy_stops_and_verifies(self):
        client = FakeClient(remains_active=True)
        result = ida.chat(client, self.local, profile(), self.output, 'hello', seconds=2, reconnects=0, cancel_on_timeout=True)
        self.assertEqual(result['verdict'], 'CANCELED')
        self.assertEqual(client.cancels, 1)

    def test_stream_for_another_agent_is_rejected_before_task_saved(self):
        client = FakeClient()
        run = self.local.new_run(profile(), session_id='404')
        other = task('TASK_STATE_WORKING')
        other['metadata']['agentId'] = 999
        with self.assertRaises(IdaError) as error:
            ida.follow(client, self.local, run, self.output, response=Response(frame({'task': other})), seconds=2)
        self.assertEqual(error.exception.code, 'AGENT_MISMATCH')
        self.assertNotIn('task_id', self.local.load_run(run['run_id'], profile()))

    def test_subscribe_command_calls_subscription_even_for_completed_task(self):
        client = FakeClient()
        client.get_task = lambda *a: task()
        self.local.save_profile(profile())
        args = ida.parser().parse_args(['--data-dir', self.tmp.name, '--profile', 'demo', 'subscribe', '--task-id', '303'])
        with patch.object(ida, 'Client', return_value=client), patch.object(ida, 'CredentialStore') as store:
            store.return_value.get.return_value = {'clientId': 'fake', 'clientSecret': 'fake'}
            result = ida.execute(args, self.output, Redactor())
        self.assertEqual(result['verdict'], 'SUCCESS')
        self.assertEqual((client.subscribes, client.sends, client.creates), (1, 0, 0))


if __name__ == '__main__':
    unittest.main()
