import { useState } from 'preact/hooks';
import { useBeans } from '../../services/beans.js';

const emptyBean = { roaster: '', name: '', codes: [] };

export function Beans() {
  const { beans, request, busy, disabled, error } = useBeans();
  const [editingId, setEditingId] = useState('');
  const [draft, setDraft] = useState(emptyBean);
  const [deletingId, setDeletingId] = useState('');
  const [codeDraft, setCodeDraft] = useState('');

  function reset() {
    setEditingId('');
    setDraft(emptyBean);
    setCodeDraft('');
  }

  async function save(event) {
    event.preventDefault();
    if (await request('save', { id: editingId, bean: draft })) reset();
  }

  return (
    <div className='mx-auto w-full max-w-4xl space-y-6'>
      <h2 className='text-3xl font-bold'>Beans</h2>
      <p className='text-base-content/70'>
        Keep your beans here. Each bean remembers its last grind size for your next brew. Link the
        barcodes or QR codes on your bags to select beans with your scanner.
      </p>
      {error && (
        <div role='alert' className='alert alert-error'>
          {error}
        </div>
      )}
      <form onSubmit={save} className='card bg-base-100 p-5'>
        <h3 className='mb-4 text-lg font-semibold'>{editingId ? 'Edit beans' : 'Add beans'}</h3>
        <div className='grid gap-4 sm:grid-cols-2'>
          {['roaster', 'name'].map(field => (
            <label key={field} className='flex flex-col gap-2'>
              <span>{field === 'roaster' ? 'Roaster' : 'Name'}</span>
              <input
                className='input w-full'
                type='text'
                required
                maxLength={100}
                value={draft[field]}
                disabled={disabled}
                onInput={event => setDraft({ ...draft, [field]: event.target.value })}
              />
            </label>
          ))}
        </div>
        <div className='mt-4 space-y-2'>
          <label htmlFor='bean-code' className='block'>
            Barcode / QR codes
          </label>
          <p className='text-base-content/70 text-sm'>
            Add up to 16 codes per bean, or use Scan new code on the machine’s bean picker.
          </p>
          {draft.codes.map(code => (
            <div key={code} className='flex items-center gap-2'>
              <span className='min-w-0 flex-1 font-mono text-sm break-all'>{code}</span>
              <button
                className='btn btn-sm'
                type='button'
                disabled={disabled}
                aria-label={`Remove code ${code}`}
                onClick={() =>
                  setDraft({ ...draft, codes: draft.codes.filter(value => value !== code) })
                }
              >
                Remove
              </button>
            </div>
          ))}
          <div className='flex gap-2'>
            <input
              id='bean-code'
              className='input min-w-0 flex-1'
              type='text'
              value={codeDraft}
              maxLength={512}
              disabled={disabled || draft.codes.length >= 16}
              placeholder='Barcode text or QR content'
              onInput={event => setCodeDraft(event.target.value)}
            />
            <button
              className='btn'
              type='button'
              disabled={
                disabled ||
                !codeDraft ||
                draft.codes.length >= 16 ||
                draft.codes.includes(codeDraft)
              }
              onClick={() => {
                setDraft({ ...draft, codes: [...draft.codes, codeDraft] });
                setCodeDraft('');
              }}
            >
              Add code
            </button>
          </div>
        </div>
        <div className='mt-4 flex gap-2'>
          <button className='btn btn-primary' type='submit' disabled={disabled}>
            {busy ? 'Saving…' : editingId ? 'Save changes' : 'Add beans'}
          </button>
          {editingId && (
            <button className='btn' type='button' onClick={reset}>
              Cancel
            </button>
          )}
        </div>
      </form>
      {!beans.length && <p className='text-base-content/70'>No beans added yet.</p>}
      <div className='space-y-3'>
        {beans.map(bean => (
          <div
            key={bean.id}
            className='card bg-base-100 flex flex-col gap-3 p-4 sm:flex-row sm:items-center'
          >
            <div className='flex-1'>
              <h3 className='font-semibold'>{bean.name}</h3>
              <p className='text-base-content/70 text-sm'>{bean.roaster}</p>
              {bean.grindSetting != null && (
                <p className='text-sm'>Last grind size: {Number(bean.grindSetting).toFixed(1)}</p>
              )}
              {!!bean.codes?.length && (
                <p className='text-sm'>
                  {bean.codes.length} linked {bean.codes.length === 1 ? 'code' : 'codes'}
                </p>
              )}
            </div>
            <button
              type='button'
              className='btn btn-sm'
              disabled={disabled}
              onClick={() => {
                setEditingId(bean.id);
                setDraft({ roaster: bean.roaster, name: bean.name, codes: bean.codes || [] });
                setCodeDraft('');
              }}
            >
              Edit
            </button>
            <button
              type='button'
              className='btn btn-sm btn-error'
              disabled={disabled}
              onClick={() => setDeletingId(bean.id)}
            >
              Remove
            </button>
            {deletingId === bean.id && (
              <div className='flex flex-wrap items-center gap-2'>
                <span className='text-sm'>Remove these beans? Past shots keep their details.</span>
                <button
                  type='button'
                  className='btn btn-sm btn-error'
                  disabled={disabled}
                  onClick={async () => {
                    if (await request('delete', { id: bean.id })) {
                      setDeletingId('');
                      if (editingId === bean.id) reset();
                    }
                  }}
                >
                  Remove beans
                </button>
                <button type='button' className='btn btn-sm' onClick={() => setDeletingId('')}>
                  Cancel
                </button>
              </div>
            )}
          </div>
        ))}
      </div>
    </div>
  );
}
