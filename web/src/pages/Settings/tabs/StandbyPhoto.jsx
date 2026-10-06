import { useEffect, useState } from 'preact/hooks';
import { photoPreview, preparePhoto } from '../../../utils/standbyPhoto.js';

const ENDPOINT = '/api/standby-photo';

export default function StandbyPhoto() {
  const [preview, setPreview] = useState('');
  const [savedPreview, setSavedPreview] = useState('');
  const [photo, setPhoto] = useState(null);
  const [busy, setBusy] = useState(true);
  const [message, setMessage] = useState('');
  const [error, setError] = useState('');
  useEffect(() => {
    const abort = new AbortController();
    (async () => {
      try {
        const response = await fetch(ENDPOINT, { cache: 'no-store', signal: abort.signal });
        if (response.status === 404) return;
        if (!response.ok) throw new Error('Could not load the saved standby photo');
        const url = photoPreview(new Uint8Array(await response.arrayBuffer()));
        setPreview(url);
        setSavedPreview(url);
      } catch (e) {
        if (e.name !== 'AbortError') setError(e.message);
      } finally {
        setBusy(false);
      }
    })();
    return () => abort.abort();
  }, []);

  async function select(event) {
    const file = event.currentTarget.files?.[0];
    if (!file) return;
    setBusy(true);
    setError('');
    setMessage('');
    setPhoto(null);
    try {
      const prepared = await preparePhoto(file);
      setPhoto(prepared.bytes);
      setPreview(prepared.preview);
    } catch (e) {
      setError(e.message);
      setPreview(savedPreview);
    } finally {
      setBusy(false);
    }
  }

  async function save(remove = false) {
    setBusy(true);
    setError('');
    setMessage('');
    try {
      const response = await fetch(
        ENDPOINT,
        remove
          ? { method: 'DELETE' }
          : {
              method: 'POST',
              headers: { 'Content-Type': 'application/octet-stream' },
              body: photo,
            },
      );
      if (!response.ok) throw new Error((await response.text()) || 'Could not save the photo');
      setPhoto(null);
      setSavedPreview(remove ? '' : preview);
      if (remove) setPreview('');
      setMessage(
        remove ? 'Photo removed. Standby uses a plain background.' : 'Standby photo saved.',
      );
    } catch (e) {
      setError(e.message);
    } finally {
      setBusy(false);
    }
  }

  return (
    <div className='mt-4 space-y-3'>
      <label htmlFor='standbyPhoto' className='block font-medium'>
        Standby photo
      </label>
      <p className='text-base-content/70 text-sm'>
        Choose a JPG, PNG, or WebP photo. It fits inside the round display; corners may be hidden.
        Photos save separately from the other settings.
      </p>
      <input
        id='standbyPhoto'
        type='file'
        accept='image/jpeg,image/png,image/webp'
        className='file-input w-full'
        disabled={busy}
        onChange={select}
      />
      {preview && (
        <img
          src={preview}
          alt='Standby photo preview'
          className='h-48 w-48 rounded-full bg-black object-contain'
        />
      )}
      <div className='flex gap-2'>
        <button
          type='button'
          className='btn btn-primary'
          disabled={busy || !photo}
          onClick={() => save()}
        >
          Save photo
        </button>
        <button
          type='button'
          className='btn btn-outline'
          disabled={busy || !savedPreview}
          onClick={() => save(true)}
        >
          Remove photo
        </button>
      </div>
      {busy && <p role='status'>Processing photo…</p>}
      {message && <p role='status'>{message}</p>}
      {error && (
        <p role='alert' className='text-error'>
          {error}
        </p>
      )}
    </div>
  );
}
