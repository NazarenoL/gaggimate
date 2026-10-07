import { useState } from 'preact/hooks';
import { useBeans } from '../../services/beans.js';

const emptyBean = { roaster: '', name: '' };

export function Beans() {
  const { beans, request, busy, disabled, error } = useBeans();
  const [editingId, setEditingId] = useState('');
  const [draft, setDraft] = useState(emptyBean);
  const [deletingId, setDeletingId] = useState('');

  function reset() {
    setEditingId('');
    setDraft(emptyBean);
  }

  async function save(event) {
    event.preventDefault();
    if (await request('save', { id: editingId, bean: draft })) reset();
  }

  return (
    <div className='mx-auto w-full max-w-4xl space-y-6'>
      <h2 className='text-3xl font-bold'>Beans</h2>
      <p className='text-base-content/70'>
        Keep your beans here. Each bean remembers its last grind size for your next brew.
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
            </div>
            <button
              type='button'
              className='btn btn-sm'
              disabled={disabled}
              onClick={() => {
                setEditingId(bean.id);
                setDraft({ roaster: bean.roaster, name: bean.name });
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
