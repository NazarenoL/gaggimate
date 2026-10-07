import { useCallback, useContext, useEffect, useState } from 'preact/hooks';
import { ApiServiceContext, machine } from './ApiService.js';

export function validGrindSize(value) {
  const number = Number(value);
  return (
    value !== '' &&
    Number.isFinite(number) &&
    number >= 1 &&
    number <= 16 &&
    Math.abs(number * 10 - Math.round(number * 10)) < 0.00001
  );
}

export function useBeans() {
  const api = useContext(ApiServiceContext);
  const connected = machine.value.connected;
  const [state, setState] = useState({ beans: [], selectedId: '', lastGrindSetting: null });
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState('');
  const [loaded, setLoaded] = useState(false);

  const request = useCallback(
    async (action, payload = {}) => {
      setBusy(true);
      setError('');
      try {
        const result = await api.request({ tp: `req:beans:${action}`, ...payload });
        if (result.error) throw new Error(result.error);
        setState({
          beans: result.beans || [],
          selectedId: result.selectedId || '',
          lastGrindSetting: result.lastGrindSetting ?? null,
        });
        setLoaded(true);
        return true;
      } catch (e) {
        setError(e.message);
        return false;
      } finally {
        setBusy(false);
      }
    },
    [api],
  );

  useEffect(() => {
    if (connected) request('list');
    else setLoaded(false);
  }, [connected, request]);

  useEffect(() => {
    const id = api.on('evt:beans:changed', message => {
      setState({
        beans: message.beans || [],
        selectedId: message.selectedId || '',
        lastGrindSetting: message.lastGrindSetting ?? null,
      });
    });
    return () => api.off('evt:beans:changed', id);
  }, [api]);

  return { ...state, busy, error, loaded, request, disabled: busy || !connected || !loaded };
}
