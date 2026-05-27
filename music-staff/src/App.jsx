import { useState } from 'react'
import Nav from './Nav'
import Home from './pages/Home'
import About from './pages/About'
import Contacts from './pages/Contacts'
import './App.css'

const PAGES = { home: Home, about: About, contacts: Contacts }

export default function App() {
  const [page, setPage] = useState('home')
  const Page = PAGES[page]
  return (
    <>
      <Nav current={page} onNavigate={setPage} />
      <Page />
    </>
  )
}
